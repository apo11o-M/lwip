/*
 * Yield the current task, and make sure it was added to rq
 * and that its vruntime was recorded correctly
 */

#include "threads/thread.h"
#include "tests/threads/cfstest.h"
#include "tests/threads/simulator.h"
#include "tests/threads/tests.h"

void
test_yield ()
{
  cfstest_set_up ();
  struct thread *initial = driver_current ();
  cfstest_advance_time (0);
  struct thread *t1 = driver_create ("t1", 0);
  cfstest_check_current (initial);
  cfstest_advance_time (2000000);
  driver_yield ();
  cfstest_check_current (t1);
  cfstest_advance_time (1000000);
  driver_yield ();
  cfstest_check_current (t1);
  pass ();
  cfstest_tear_down ();

}