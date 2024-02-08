#include "threads/scheduler.h"
#include "threads/cpu.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "list.h"
#include "threads/spinlock.h"
#include <debug.h>

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */

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
void
sched_init (struct ready_queue *curr_rq)
{
  curr_rq->min_vruntime = 0;
  list_init (&curr_rq->ready_list);
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
sched_unblock (struct ready_queue *rq_to_add, struct thread *t, int initial)
{

  /* Update thread's vruntime_0*/
  // if called from wake_up_new_thread
  if (initial == 1){
    // assign threads initial virtual runtime to the cpu's minimum virtual runtime
    t->vruntime_0=t->cpu->rq.min_vruntime;
  }
  // if called from thread_unblock
  else {
    // assign to maximum between threads current virtual runtime and cpu's min_runtime - 2E7
    uint64_t adjusted_min_vruntime = t->cpu->rq.min_vruntime - 20000000;
    t->vruntime_0 = (t->vruntime > adjusted_min_vruntime) ?  t->vruntime : adjusted_min_vruntime;
  }
  // assign vruntime to initial vruntime
  t->vruntime = t->vruntime_0;

  list_push_back (&rq_to_add->ready_list, &t->elem);
  rq_to_add->nr_ready++;

  /* CPU is idle */
  if (!rq_to_add->curr)
    {
      return RETURN_YIELD;
    }
  return RETURN_NONE;
}

/* Called from thread_yield ().
   Current thread is about to yield.  Add it to the ready list

   Current ready queue is locked upon entry.
 */
void
sched_yield (struct ready_queue *curr_rq, struct thread *current)
{
  list_push_back (&curr_rq->ready_list, &current->elem);
  curr_rq->nr_ready ++;
}

/* Called from next_thread_to_run ().
   Find the next thread to run and remove it from the ready list
   Return NULL if the ready list is empty.

   If the thread returned is different from the thread currently
   running, a context switch will take place.

   Called with current ready queue locked.
 */
struct thread *
sched_pick_next (struct ready_queue *curr_rq)
{

  if (list_empty (&curr_rq->ready_list))
    return NULL;

  // TODO: Modify to select next thread based on lowest vruntime, call for_each_thread
  struct thread *ret = list_entry(list_pop_front (&curr_rq->ready_list), struct thread, elem);
  curr_rq->nr_ready--;
  return ret;
}


/* 
 * function to sum all the weights of threads in running queue
 */
static void
sum_weights (struct thread *t, void *aux)
{
  *(int*) aux = *(int*) aux + prio_to_weight[t->nice];
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
sched_tick (struct ready_queue *curr_rq, struct thread *current)
{
  // update virtual runtime
  current->vruntime = current->vruntime_0 + curr_rq->thread_ticks * prio_to_weight[0] / prio_to_weight[current->nice];

  // calculate ideal runtime
  int sum_of_weights = 0;
  thread_foreach(sum_weights, &sum_of_weights);
  sum_of_weights += prio_to_weight[current->nice];
  uint64_t ideal_runtime = 4000000 * (list_size(&(curr_rq->ready_list)) + 1) * prio_to_weight[current->nice] / sum_of_weights;

  /* Enforce preemption. */
  // check if current thread vruntime is longer than ideal runtime, yield if so
  if (++curr_rq->thread_ticks >= TIME_SLICE && current->vruntime > ideal_runtime)
    {
      /*update ready queue min_vruntime*/
      // if there are ready threads in queue
      if(curr_rq->nr_ready >0){
      struct thread* first_thread_in_rq = list_entry(list_front(&(curr_rq->ready_list)), struct thread, elem);
      uint64_t lowest_vruntime_in_rq = first_thread_in_rq->vruntime;
      curr_rq->min_vruntime = (current->vruntime < lowest_vruntime_in_rq) ?  current->vruntime : lowest_vruntime_in_rq;
      }
      // if no ready threads in queue
      else{
        curr_rq->min_vruntime = current->vruntime;
      }
      
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
void
sched_block (struct ready_queue *rq UNUSED, struct thread *current UNUSED) {}

/*
 * Pull threads from another CPU's ready queue and add it to the current CPU's 
 * ready queue.
 */
void sched_load_balance(void) 
{
  uint64_t busiest_cpu_load = 0;
  uint8_t busiest_cpu_id = 0;

  // Go through all threads in the ready queue and sum up their weights to find
  // the busiest CPU (highest load)
  struct cpu *c;
  for (c = cpus; c < cpus + ncpu; c++) {
    spinlock_acquire(&c->rq.lock);

    uint64_t curr_cpu_load = 0;
    // Further optimization can be done by updating the cpu_load whenever a
    // thread is added/removed from the ready queue
    struct list_elem *e;
    for (e = list_begin(&c->rq.ready_list); e != list_end(&c->rq.ready_list); e = list_next(e)) {
      struct thread *t = list_entry(e, struct thread, elem);
      curr_cpu_load += prio_to_weight[t->nice];
    }
    c->rq.cpu_load = curr_cpu_load;

    if (curr_cpu_load > busiest_cpu_load) {
      busiest_cpu_load = c->rq.cpu_load;
      busiest_cpu_id = c->id;
    }
    spinlock_release(&c->rq.lock);
  }

  // make sure to not pull tasks from oneself as it's already the busiest
  if (busiest_cpu_id == get_cpu()->id) return;

  lock_own_ready_queue();
  spinlock_acquire(&cpus[busiest_cpu_id].rq.lock);

  // by this point, all cpus' curr_load are updated
  uint64_t imbalance = (busiest_cpu_load - get_cpu()->rq.cpu_load) / 2;

  // if the load is too imbalance across CPUs, pull threads from the busiest 
  // CPU to the current cpu.
  if (imbalance * 4 >= busiest_cpu_load) {
    // continue to pull tasks from the busiest CPU until the load is balanced
    while (imbalance > 0) {
      struct thread *t = sched_pick_next(&cpus[busiest_cpu_id].rq);
      if (t == NULL) break;
      list_push_back(&get_cpu()->rq.ready_list, &t->elem);
      get_cpu()->rq.nr_ready++;

      // make sure to update the vruntime of the migrated thread as vruntime is
      // cpu dependent
      t->vruntime_0 = t->vruntime - cpus[busiest_cpu_id].rq.min_vruntime + get_cpu()->rq.min_vruntime;

      imbalance -= prio_to_weight[t->nice];
    }
  }

  spinlock_release(&cpus[busiest_cpu_id].rq.lock);
  unlock_own_ready_queue();
}
