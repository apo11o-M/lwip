/*
 * Test printing under when interrupts are disabled, and
 * console_use_spinlock () is called.
 */
#include "threads/thread.h"
#include "threads/interrupt.h"
#include <debug.h>
#include <stdio.h>
#include "tests/misc/misc_tests.h"
#include "threads/synch.h"
#include <debug.h>
#include "lib/atomic-ops.h"
#include "devices/intq.h"
#include "lib/kernel/console.h"

#define NUMTHREADS 4

static struct semaphore sema;
static int start = 0;
static void
print (void *args UNUSED)
{
  while (!start) {
    ;  
  }
  console_use_spinlock ();
  intr_disable_push ();
  printf ("Thread %d: Printing Line 1\n", thread_current ()->tid);
  printf ("Thread %d: Printing Line 2\n", thread_current ()->tid);
  printf ("Thread %d: Printing Line 3\n", thread_current ()->tid);
  printf ("Thread %d: Printing Line 4\n", thread_current ()->tid);
  printf ("Thread %d: Printing Line 5\n", thread_current ()->tid);
  intr_disable_pop ();
  sema_up (&sema);
}

void
test_cli_print (void)
{
  sema_init (&sema, 0);
  unsigned int i;
  for (i = 0; i < NUMTHREADS; i++)
    {
      thread_create ("print1", 0, print, 0);
    }
  atomic_xchg (&start, 1);
  for (i = 0; i < NUMTHREADS; i++)
    {
      sema_down (&sema);
    }
  pass ();
}
