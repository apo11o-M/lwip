/*
 * Test inter-processor interrupts
 */
#include "threads/interrupt.h"
#include "lib/atomic-ops.h"
#include <debug.h>
#include "devices/lapic.h"
#include "tests/misc/misc_tests.h"
#include "threads/mp.h"
#include "devices/timer.h"

#define IPI_NUM 4

static int flag = 0;

static void
ipi_test (struct intr_frame *f UNUSED)
{
  atomic_inci (&flag);
}

static void
register_test_ipi (void)
{
  intr_register_ipi (T_IPI + IPI_NUM, ipi_test, "#IPI TEST");
}

void
test_ipi (void)
{
  failIfFalse (ncpu > 1, "This test >1 cpu running");
  register_test_ipi ();
  lapicsendallbutself (IPI_NUM);
  timer_sleep (5);
  failIfFalse (flag == (int) (ncpu - 1), "Other CPUs did not respond to IPI");
  pass ();
}

static int numstarted = 0;
/*
 * Disable interrupts, so it cannot respond to IPI
 */
static void
AP_disable (void *aux UNUSED)
{
  intr_disable ();
  atomic_inci (&numstarted);
  while (1)
    ;
}
/*
 * This test assumes that threads are assigned to
 * CPUs in a round robin fashion. Attempt to disable
 * interrupts in all CPUs. 
 * Tests that IPI is blocked by disabling interrupts
 */
void
test_ipi_blocked (void)
{
  failIfFalse (ncpu > 1, "This test > 1 cpu running");
  register_test_ipi ();
  unsigned int i;
  for (i = 0; i < ncpu - 1; i++)
    {
      thread_create ("test_ipi", 0, AP_disable, NULL);
    }
  while (atomic_load (&numstarted) < (int) (ncpu - 1))
    ;
  lapicsendallbutself (IPI_NUM);
  timer_sleep (5);
  failIfFalse (flag == 0, "Other CPUs responded to IPI, despite interrupts "
	       "being off");
  pass ();
}

void
test_ipi_all (void)
{
  failIfFalse (ncpu > 1, "This test > 1 cpu running");
  register_test_ipi ();
  lapicsendall (IPI_NUM);
  timer_sleep (5);
  failIfFalse (flag == (int) ncpu, "One of the CPUs did not respond to IPI");
  pass ();
}
