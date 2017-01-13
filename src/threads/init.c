#include "threads/init.h"
#include <console.h>
#include <debug.h>
#include <inttypes.h>
#include <stdbool.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devices/kbd.h"
#include "devices/input.h"
#include "devices/pci.h"
#include "devices/usb.h"
#include "devices/serial.h"
#include "devices/shutdown.h"
#include "devices/timer.h"
#include "devices/vga.h"
#include "devices/rtc.h"
#include "devices/lapic.h"
#include "devices/ioapic.h"
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "threads/gdt.h"
#include "threads/tss.h"
#include "threads/mp.h"
#include "threads/ipi.h"
#include "threads/cpu.h"
#ifdef USERPROG
#include "userprog/process.h"
#include "userprog/exception.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#else
#include "tests/threads/tests.h"
#include "tests/threads/schedtest.h"
#include "tests/misc/misc_tests.h"
#endif
#ifdef FILESYS
#include "devices/block.h"
#include "devices/ide.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#endif
#include "lib/kernel/x86.h"
#include "lib/atomic-ops.h"

/* Page directory with kernel mappings only. */
uint32_t *init_page_dir;
#ifdef FILESYS
/* -f: Format the file system? */
static bool format_filesys;

/* -filesys, -scratch, -swap: Names of block devices to use,
 overriding the defaults. */
static const char *filesys_bdev_name;
static const char *scratch_bdev_name;
#ifdef VM
static const char *swap_bdev_name;
#endif
#endif /* FILESYS */

/* -ul: Maximum number of pages to put into palloc's user pool. */
static size_t user_page_limit = SIZE_MAX;

static void bss_init (void);
static void paging_init (void);
static void pci_zone_init (void);
static void lapic_zone_init (void);
static char ** read_command_line (void);
static char ** parse_options (char **argv);
static void run_actions (char **argv);
static void usage (void);
static void startothers (void);
static void mpenter (void);

#ifdef FILESYS
static void locate_block_devices (void);
static void locate_block_device (enum block_type, const char *name);
#endif

int main (void) NO_RETURN;

/* Pintos main program. */
int
main (void)
{
  char **argv;

  /* Clear BSS. */
  bss_init ();
  cpu_startedothers = 0;
  cpu_can_acquire_spinlock = 0;

  /* Initialize memory system. */
  palloc_init (user_page_limit);
  malloc_init ();
  paging_init ();
  mp_init ();        /* collect info about this machine */
  lapic_init ();
  /* Initialize ourselves as a thread so we can use locks */
  thread_init ();

  /* Segmentation. */
  gdt_init ();
  tss_init ();
  
  /* Per-CPU variables are set up, so it's safe to acquire a lock */
  cpu_can_acquire_spinlock = 1;
  
  console_init ();
  /* Break command line into arguments and parse options. */
  argv = read_command_line ();
  argv = parse_options (argv);

  /* Greet user. */
  printf ("CPU %d is up\n", get_cpu ()->id);
  printf ("Pintos booting with %'"PRIu32" kB RAM...\n",
	  init_ram_pages * PGSIZE / 1024);

  /* Initialize interrupt handlers. */
  intr_init ();
  timer_init ();
  kbd_init ();
  pci_init ();
  input_init ();
  ipi_init ();
#ifdef USERPROG
  exception_init ();
  syscall_init ();
#endif

  serial_init_queue ();
  /* Start thread scheduler and enable interrupts. */
  thread_start ();
  
  /* Scheduler and interrupt handlers are set up, so 
     interrupts can be enabled now. */
  intr_enable ();
  timer_calibrate ();
/*  usb_init (); */
#ifdef FILESYS
  /* Initialize file system. */
  usb_storage_init ();
  ide_init ();
  locate_block_devices ();
  filesys_init (format_filesys);
#endif
  
  /* start other processors */
  startothers ();     
  printf ("Boot complete.\n");
  
  /* Run actions specified on kernel command line. */
  run_actions (argv);
  /* Finish up. */
  shutdown ();
  thread_exit ();
}

/* Clear the "BSS", a segment that should be initialized to
   zeros.  It isn't actually stored on disk or zeroed by the
   kernel loader, so we have to zero it ourselves.

   The start and end of the BSS segment is recorded by the
   linker as _start_bss and _end_bss.  See kernel.lds. */
static void
bss_init (void) 
{
  extern char _start_bss, _end_bss;
  memset (&_start_bss, 0, &_end_bss - &_start_bss);
}

/* Populates the base page directory and page table with the
   kernel virtual mapping, and then sets up the CPU to use the
   new page directory.  Points init_page_dir to the page
   directory it creates. */
static void
paging_init (void)
{
  uint32_t *pd, *pt;
  size_t page;
  extern char _start, _end_kernel_text;

  pd = init_page_dir = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  pt = NULL;
  for (page = 0; page < init_ram_pages; page++) 
    {
      uintptr_t paddr = page * PGSIZE;
      char *vaddr = ptov (paddr);
      size_t pde_idx = pd_no (vaddr);
      size_t pte_idx = pt_no (vaddr);
      bool in_kernel_text = &_start <= vaddr && vaddr < &_end_kernel_text;

      if (pd[pde_idx] == 0)
        {
          pt = palloc_get_page (PAL_ASSERT | PAL_ZERO);
          pd[pde_idx] = pde_create_kernel (pt);
        }

      pt[pte_idx] = pte_create_kernel (vaddr, !in_kernel_text);
    }

  pci_zone_init ();
  lapic_zone_init ();
  lcr3 (vtop (init_page_dir));
}

/* initialize PCI zone at PCI_ADDR_ZONE_BEGIN - PCI_ADDR_ZONE_END*/
static void
pci_zone_init (void)
{
  int i;
  for (i = 0; i < PCI_ADDR_ZONE_PDES; i++)
    {
      size_t pde_idx = pd_no ((void *) PCI_ADDR_ZONE_BEGIN) + i;
      uint32_t pde;
      void *pt;

      pt = palloc_get_page (PAL_ASSERT | PAL_ZERO);
      pde = pde_create_kernel (pt);
      init_page_dir[pde_idx] = pde;
    }
}

/* Create a 1:1 mapping starting at APIC_ZONE_BEGIN so that we may
   access the memory-mapped lapic */
static void
lapic_zone_init (void)
{
  uint32_t *pd, *pt;
  size_t page;
  pd = init_page_dir;
  pt = NULL;
  for (page = 0; page < APIC_ZONE_PDES; page++)
    {
      uintptr_t paddr = APIC_ZONE_BEGIN + page * PGSIZE;
      char *vaddr = (void *) paddr;
      size_t pde_idx = pd_no (vaddr);
      size_t pte_idx = pt_no (vaddr);
      if (pd[pde_idx] == 0)
	{
	  pt = palloc_get_page (PAL_ASSERT | PAL_ZERO);
	  pd[pde_idx] = pde_create_kernel (pt);
	}
      uint32_t mapping = pte_create_kernel_identity (vaddr, true);
      pt[pte_idx] = mapping;
    }
}

/* Other CPUs jump here from startother.S. */
static void
mpenter (void)
{
  lcr3 (vtop (init_page_dir));
  lapic_init ();
  thread_AP_init ();
  gdt_init ();
  tss_init ();
  intr_load_idt ();
  printf ("CPU %d is up\n", get_cpu ()->id);
  thread_start ();
  atomic_xchg (&get_cpu ()->started, 1);     /* tell startothers() we're up */
  /* Wait for other CPUs to start up */
  while (!atomic_load (&cpu_startedothers))
    ;
  /* Schedule threads*/
  thread_AP_yield ();
  PANIC("Should not come back here!");
}

/* Start the non-boot (AP) processors. */
static void
startothers (void)
{
  printf("Using per-cpu local queues\n");
  intr_disable_push ();
  cpu_can_acquire_spinlock = 0;
  extern uint8_t _binary___threads_startother_start[],
      _binary___threads_startother_size[];
  uint8_t *code;
  struct cpu *c;
  char *stack;

  /* Write entry code to unused memory at 0x7000. */
  /* The linker has placed the image of entryother.S in */
  /* _binary_entryother_start. */
  code = ptov (0x7000);
  memmove (code, _binary___threads_startother_start,
	   (uint32_t) _binary___threads_startother_size);

  for (c = cpus; c < cpus + ncpu; c++)
    {
      if (c == cpus + lapic_get_cpuid ())     /* We've started already. */
	continue;

      /* Tell entryother.S what stack to use and where to enter */
      stack = palloc_get_page (PAL_ASSERT | PAL_ZERO);
      *(void**) (code - 4) = stack + PGSIZE;
      *(void**) (code - 8) = mpenter;

      lapicstartap (c->id, vtop (code));

      /* wait for cpu to finish mpmain() */
      while (!atomic_load (&c->started))
	;
    }
  /* Each AP is woken up sequentially and blocks until startedothers
   * is true. Interrupts are disabled here, therefore AP's do not 
   * need to hold spinlocks. Attempting to do so will cause a triple
   * fault because spinlocks at the minimum requires the lapic
   * to be initialized and usually also requires the GDT to be set up
   * with Per-CPU variables, although the latter can be circumvented by 
   * polling the lapic instead for the cpu ID.
   */
  atomic_store (&cpu_can_acquire_spinlock, 1);
  atomic_store (&cpu_startedothers, 1);
  intr_disable_pop ();
}

/* Breaks the kernel command line into words and returns them as
   an argv-like array. */
static char **
read_command_line (void) 
{
  static char *argv[LOADER_ARGS_LEN / 2 + 1];
  char *p, *end;
  int argc;
  int i;

  argc = *(uint32_t *) ptov (LOADER_ARG_CNT);
  p = ptov (LOADER_ARGS);
  end = p + LOADER_ARGS_LEN;
  for (i = 0; i < argc; i++) 
    {
      if (p >= end)
        PANIC ("command line arguments overflow");

      argv[i] = p;
      p += strnlen (p, end - p) + 1;
    }
  argv[argc] = NULL;

  /* Print kernel command line. */
  printf ("Kernel command line:");
  for (i = 0; i < argc; i++)
    if (strchr (argv[i], ' ') == NULL)
      printf (" %s", argv[i]);
    else
      printf (" '%s'", argv[i]);
  printf ("\n");

  return argv;
}

/* Parses options in ARGV[]
   and returns the first non-option argument. */
static char **
parse_options (char **argv) 
{
  for (; *argv != NULL && **argv == '-'; argv++)
    {
      char *save_ptr;
      char *name = strtok_r (*argv, "=", &save_ptr);
      char *value = strtok_r (NULL, "", &save_ptr);
      
      if (!strcmp (name, "-h"))
        usage ();
      else if (!strcmp (name, "-q"))
        shutdown_configure (SHUTDOWN_POWER_OFF);
      else if (!strcmp (name, "-r"))
        shutdown_configure (SHUTDOWN_REBOOT);
#ifdef FILESYS
      else if (!strcmp (name, "-f"))
        format_filesys = true;
      else if (!strcmp (name, "-filesys"))
        filesys_bdev_name = value;
      else if (!strcmp (name, "-scratch"))
        scratch_bdev_name = value;
#ifdef VM
      else if (!strcmp (name, "-swap"))
        swap_bdev_name = value;
#endif
#endif
      else if (!strcmp (name, "-rs"))
        random_init (atoi (value));
#ifdef USERPROG
      else if (!strcmp (name, "-ul"))
        user_page_limit = atoi (value);
#endif
      else
        PANIC ("unknown option `%s' (use -h for help)", name);
    }

  /* Initialize the random number generator based on the system
     time.  This has no effect if an "-rs" option was specified.

     When running under Bochs, this is not enough by itself to
     get a good seed value, because the pintos script sets the
     initial time to a predictable value, not to the local time,
     for reproducibility.  To fix this, give the "-r" option to
     the pintos script to request real-time execution. */
  random_init (rtc_get_time ());
  
  return argv;
}

/* Runs the task specified in ARGV[1]. */
static void
run_task (char **argv)
{
  const char *task = argv[1];
  
  printf ("Executing '%s':\n", task);
#ifdef USERPROG
  process_wait (process_execute (task));
#elif MISC
  run_misc_test (task);
#else
  run_test (task);
#endif
  printf ("Execution of '%s' complete.\n", task);
}

/* Executes all of the actions specified in ARGV[]
   up to the null pointer sentinel. */
static void
run_actions (char **argv) 
{
  /* An action. */
  struct action 
    {
      char *name;                       /* Action name. */
      int argc;                         /* # of args, including action name. */
      void (*function) (char **argv);   /* Function to execute action. */
    };

  /* Table of supported actions. */
  static const struct action actions[] = 
    {
      {"run", 2, run_task},
#ifdef FILESYS
      {"ls", 1, fsutil_ls},
      {"cat", 2, fsutil_cat},
      {"rm", 2, fsutil_rm},
      {"extract", 1, fsutil_extract},
      {"append", 2, fsutil_append},
#endif
      {NULL, 0, NULL},
    };

  while (*argv != NULL)
    {
      const struct action *a;
      int i;

      /* Find action name. */
      for (a = actions; ; a++)
        if (a->name == NULL)
          PANIC ("unknown action `%s' (use -h for help)", *argv);
        else if (!strcmp (*argv, a->name))
          break;

      /* Check for required arguments. */
      for (i = 1; i < a->argc; i++)
        if (argv[i] == NULL)
          PANIC ("action `%s' requires %d argument(s)", *argv, a->argc - 1);

      /* Invoke action and advance. */
      a->function (argv);
      argv += a->argc;
    }
  
}

/* Prints a kernel command line help message and powers off the
   machine. */
static void
usage (void)
{
  printf ("\nCommand line syntax: [OPTION...] [ACTION...]\n"
          "Options must precede actions.\n"
          "Actions are executed in the order specified.\n"
          "\nAvailable actions:\n"
#ifdef USERPROG
          "  run 'PROG [ARG...]' Run PROG and wait for it to complete.\n"
#else
          "  run TEST           Run TEST.\n"
#endif
#ifdef FILESYS
          "  ls                 List files in the root directory.\n"
          "  cat FILE           Print FILE to the console.\n"
          "  rm FILE            Delete FILE.\n"
          "Use these actions indirectly via `pintos' -g and -p options:\n"
          "  extract            Untar from scratch device into file system.\n"
          "  append FILE        Append FILE to tar file on scratch device.\n"
#endif
          "\nOptions:\n"
          "  -h                 Print this help message and power off.\n"
          "  -q                 Power off VM after actions or on panic.\n"
          "  -r                 Reboot after actions.\n"
#ifdef FILESYS
          "  -f                 Format file system device during startup.\n"
          "  -filesys=BDEV      Use BDEV for file system instead of default.\n"
          "  -scratch=BDEV      Use BDEV for scratch instead of default.\n"
#ifdef VM
          "  -swap=BDEV         Use BDEV for swap instead of default.\n"
#endif
#endif
          "  -rs=SEED           Set random number seed to SEED.\n"
#ifdef USERPROG
          "  -ul=COUNT          Limit user memory to COUNT pages.\n"
#endif
          );
  shutdown_power_off ();
}

#ifdef FILESYS
/* Figure out what block devices to cast in the various Pintos roles. */
static void
locate_block_devices (void)
{
  locate_block_device (BLOCK_FILESYS, filesys_bdev_name);
  locate_block_device (BLOCK_SCRATCH, scratch_bdev_name);
#ifdef VM
  locate_block_device (BLOCK_SWAP, swap_bdev_name);
#endif
}

/* Figures out what block device to use for the given ROLE: the
   block device with the given NAME, if NAME is non-null,
   otherwise the first block device in probe order of type
   ROLE. */
static void
locate_block_device (enum block_type role, const char *name)
{
  struct block *block = NULL;

  if (name != NULL)
    {
      block = block_get_by_name (name);
      if (block == NULL)
        PANIC ("No such block device \"%s\"", name);
    }
  else
    {
      for (block = block_first (); block != NULL; block = block_next (block))
        if (block_type (block) == role)
          break;
    }

  if (block != NULL)
    {
      printf ("%s: using %s\n", block_type_name (role), block_name (block));
      block_set_role (role, block);
    }
}
#endif
