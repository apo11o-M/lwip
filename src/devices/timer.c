#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/spinlock.h"
#include "threads/thread.h"
#include "threads/cpu.h"
#include "devices/trap.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;
/* The current time wall clock time in nanoseconds */
static uint64_t cur_time = 0;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);


static void wake_check(struct cpu *c);

static void add_sleeping_thread(struct thread *t);



/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  intr_register_ext (0x20 + IRQ_TIMER, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) 
{
  int64_t t = ticks;
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  ASSERT (intr_get_level () == INTR_ON);
  
  struct thread *t = thread_current();
  ASSERT (t->status == THREAD_RUNNING);
  t->when_to_wake = ticks + timer_ticks();
  t->sleeping = true;

  // lock_acquire(&list_lock);
  // add_sleeping_thread(t);
  // lock_release(&list_lock);

  /* Acquire the cpu's spin lock to add waiting thread */

  intr_disable_push();
  struct cpu *c = get_cpu();
  intr_enable_pop();

  spinlock_acquire(&c->cpu_spinlock);

  add_sleeping_thread(t);
  thread_block(&c->cpu_spinlock);


  spinlock_release(&c->cpu_spinlock);

}


void add_sleeping_thread(struct thread *t) {
  
  struct cpu *c = get_cpu();
  /* If the thread list is empty */
  if (list_empty(&c->sleeping_threads)){
    list_push_back(&c->sleeping_threads, &t->elem);
    return;
  }
  /* Add the thread by placing it in order of when it should be completed */
  struct list_elem *e;
  for (e = list_begin (&c->sleeping_threads); e != list_end (&c->sleeping_threads);
       e = list_next (e))
    {
      struct thread *t2 = list_entry (e, struct thread, elem);
      if (t->when_to_wake < t2->when_to_wake){
        list_insert(e, &t->elem);
        return;
      }
    }
  

}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  /* CPU 0 is in charge of maintaining wall-clock time */
  struct cpu *c = get_cpu();
  if (c->id == 0) 
    {
      ticks++;
      timer_settime (timer_ticks () * NSEC_PER_SEC / TIMER_FREQ);
    }
    
  /* decrement the sleep counter for each sleeping thread */
  // thread_foreach(&wake_check, NULL);

  // spinlock_acquire(&c->cpu_spinlock);

  wake_check(c);

  // spinlock_release(&c->cpu_spinlock);

  thread_tick ();
}

/* Decrement a thread's sleep counter, and unblock it if the counter reaches 0. */
static void wake_check(struct cpu *c){
  
  /* If the list is empty return */
  if (list_empty(&c->sleeping_threads)){
    return;
  }
 

  /* If the first element of the list is not ready, none will be so return */
  struct thread *t = list_entry (list_begin (&c->sleeping_threads), struct thread, elem);
  if (t->when_to_wake > timer_ticks()){
    return;
  }

  /* If the first element is ready, unblock it and remove it from the list */
  struct list_elem *e = list_front(&c->sleeping_threads);
  struct list_elem *next;
  while (e != list_end (&c->sleeping_threads)){
    t = list_entry (e, struct thread, elem);
    next = list_next(e);
    if (t->when_to_wake <= timer_ticks()){
      list_remove(e);
      thread_unblock(t);
    }
    e = next;
  }


}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      timer_sleep (ticks); 
      busy_wait ((num * TIMER_FREQ / denom) % 1);
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}

/*
 * Set the current (wall-clock) time.
 * This is done by the simulation framework during testing, and
 * done by CPU0 during the actual execution.
 */
void
timer_settime (uint64_t time) 
{
  cur_time = time;
}

/* Return current time in nanosec units */
uint64_t
timer_gettime ()
{
  return cur_time;
}
