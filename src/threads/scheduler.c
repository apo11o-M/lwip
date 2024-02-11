#include "threads/scheduler.h"
#include "threads/cpu.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "list.h"
#include "threads/spinlock.h"
#include <debug.h>
#include <stdio.h>
#include "devices/timer.h"
/* Scheduling. */
#define TIME_SLICE 4 /* # of timer ticks to give each thread. */

/*
 * In the provided baseline implementation, threads are kept in an unsorted list.
 *
 * Threads are added to the back of the list upon creation.
 * The thread at the front is picked for scheduling.
 * Upon preemption, the current thread is added to the end of the queue
 * (in sched_yield), creating a round-robin policy if multiple threads
 * are in the ready queue.
 * Preemption occurs every TIME_SLICE ticks.
 */

/* Called from thread_init () and thread_init_on_ap ().
   Initializes data structures used by the scheduler.

   This function is called very early.  It is unsafe to call
   thread_current () at this point.
 */
void sched_init(struct ready_queue *curr_rq)
{
  curr_rq->min_vruntime = 0;
  list_init(&curr_rq->ready_list);
}


void insert_sorted(struct ready_queue *curr_rq, struct thread *current, bool new_thread UNUSED)
{
    // if list is not empty
  // insert list in sorted fashion by ascending vruntime
  if (curr_rq->nr_ready > 0)
  {
    struct list_elem *curr_elem = list_front(&curr_rq->ready_list);
    while (curr_elem != list_tail(&curr_rq->ready_list) && 
      (((current->vruntime) > list_entry(curr_elem, struct thread, elem)->vruntime) || 
        (((current->vruntime) == list_entry(curr_elem, struct thread, elem)->vruntime && 
            (current->tid) > list_entry(curr_elem, struct thread, elem)->tid))))
    {
      curr_elem = list_next(curr_elem);
    }
    list_insert(curr_elem, &current->elem);
  }
  else
  {
    list_push_back(&curr_rq->ready_list, &current->elem);
  }
  curr_rq->nr_ready++;
}
/* Called from thread.c:wake_up_new_thread () and
   thread_unblock () with the CPU's ready queue locked.
   rq is the ready queue that t should be added to when
   it is awoken. It is not necessarily the current CPU.

   If called from wake_up_new_thread (), initial will be 1.
   If called from thread_unblock (), initial will be 0.

   Returns RETURN_YIELD if the CPU containing rq should
   be rescheduled when this function returns, else returns
   RETURN_NONE */
enum sched_return_action
sched_unblock(struct ready_queue *rq_to_add, struct thread *t, int initial)
{
  bool should_yield = false;
  /* Update thread's vruntime_0*/
  // if called from wake_up_new_thread
  if (initial == 1)
  {
    // assign threads initial virtual runtime to the cpu's minimum virtual runtime
    t->vruntime = t->cpu->rq.min_vruntime;
  }
  // if called from thread_unblock
  else
  {
    // assign to maximum between threads current virtual runtime and cpu's min_runtime - 2E8
    uint64_t adjusted_min_vruntime = t->cpu->rq.min_vruntime - 20000000;
    t->vruntime = (t->vruntime > adjusted_min_vruntime || t->cpu->rq.min_vruntime <= 20000000) ? t->vruntime : adjusted_min_vruntime;
    should_yield = true;
  }
  t->last_update = timer_gettime();


  // insert new thread into ready queue
  insert_sorted(rq_to_add, t, (bool)initial);

  /* CPU is idle */
  if (!rq_to_add->curr || should_yield)
  {
    return RETURN_YIELD;
  }
  return RETURN_NONE;
}

/* Called from thread_yield ().
   Current thread is about to yield.  Add it to the ready list

   Current ready queue is locked upon entry.
 */
void sched_yield(struct ready_queue *curr_rq, struct thread *current)
{
  update_vruntime(current);
  insert_sorted(curr_rq, current, false);
  update_minvruntime(curr_rq, current);
  
}

/* Called from next_thread_to_run ().
   Find the next thread to run and remove it from the ready list
   Return NULL if the ready list is empty.

   If the thread returned is different from the thread currently
   running, a context switch will take place.

   Called with current ready queue locked.
 */
struct thread *
sched_pick_next(struct ready_queue *curr_rq)
{

  if (list_empty(&curr_rq->ready_list))
    return NULL;
  struct thread *ret = list_entry(list_pop_front(&curr_rq->ready_list), struct thread, elem);
  ret->last_update = timer_gettime();
  curr_rq->nr_ready--;
  return ret;
}

/*
 * function to sum all the weights of threads in running queue
 */
int sum_ready_weights(struct ready_queue *rq)
{
  int sum = 0;
  struct list_elem *e;
  for (e = list_begin(&rq->ready_list); e != list_end(&rq->ready_list); e = list_next(e))
  {
    struct thread *t = list_entry(e, struct thread, elem);
    sum += prio_to_weight[t->nice + 20];
  }
  return sum;
}

// update vruntime of passed thread
void update_vruntime(struct thread *current)
{
  current->vruntime += (timer_gettime() - current->last_update) * prio_to_weight[0+20] / prio_to_weight[current->nice + 20];
  current->last_update = timer_gettime();
}

/*update ready queue min_vruntime*/
void update_minvruntime(struct ready_queue *curr_rq, struct thread *current){

    // iterate through ready queue


    // if there are ready threads in queue
    uint64_t new_min_vruntime;
    if (curr_rq->nr_ready > 0)
    {
      struct thread *first_thread_in_rq = list_entry(list_front(&(curr_rq->ready_list)), struct thread, elem);
      uint64_t lowest_vruntime_in_rq = first_thread_in_rq->vruntime;
      new_min_vruntime = (current->vruntime < lowest_vruntime_in_rq) ? current->vruntime : lowest_vruntime_in_rq;
    }
    // if no ready threads in queue
    else
    {
      new_min_vruntime = current->vruntime;
    }

    // if new suggested minimum vruntime is larger than current min vruntime
    if (new_min_vruntime > curr_rq->min_vruntime){
      curr_rq->min_vruntime = new_min_vruntime;
    }
}
/* Called from thread_tick ().
 * Ready queue rq is locked upon entry.
 *
 * Check if the current thread has finished its timeslice,
 * arrange for its preemption if it did.
 *
 * Returns RETURN_YIELD if current thread should yield
 * when this function returns, else returns RETURN_NONE.
 */
enum sched_return_action
sched_tick(struct ready_queue *curr_rq, struct thread *current)
{
  // calculate ideal runtime
  int sum_of_weights = sum_ready_weights(curr_rq);
  // include current thread weight
  sum_of_weights += prio_to_weight[current->nice + 20];
  update_minvruntime(curr_rq, current);
  // unsigned long ideal_runtime = (4000000.0 * (double)(curr_rq->nr_ready + 1.0) * prio_to_weight[current->nice + 20]);
  uint64_t ideal_runtime = ((uint64_t)4000000 * (curr_rq->nr_ready + 1) * prio_to_weight[current->nice + 20]);
  /* Enforce preemption. */
  // check if current thread vruntime is longer than ideal runtime, yield if so
  if (((timer_gettime() - current->last_update)) * sum_of_weights >= ideal_runtime)
  {

    /* Start a new time slice. */
    curr_rq->thread_ticks = 0;

    return RETURN_YIELD;
  }
  return RETURN_NONE;
}

/* Called from thread_block (). The base scheduler does
   not need to do anything here, but your scheduler may.

   'cur' is the current thread, about to block.
 */
void sched_block(struct ready_queue *rq, struct thread *current)
{

  update_vruntime(current);
  update_minvruntime(rq, current);
}
