#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"


static void syscall_handler (struct intr_frame *);
static void halt();
static void exit(int status);
static int exec(const char *cmd_line);
static int wait(int pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);


void
syscall_init (void) 
{
  printf("syscall_init\n");
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{

  int syscall_number = *(int *)f->esp;
  // argument offsets
  // arg1 = f->esp + 4
  // arg2 = f->esp + 8
  // arg3 = f->esp + 12
  // files are *char **
  // ints are *int *
  switch (syscall_number)
  {
    case SYS_HALT:
    //pintos -v -k -T 60 --smp 2 --kvm  --filesys-size=2 -p tests/userprog/halt -a halt -- -q  -f run halt
     
      halt();
      break;
    case SYS_EXIT:
      exit(*(int *)(f->esp + 4));
      break;
    case SYS_EXEC:
      exec(*(char **)(f->esp + 4));
      break;
    case SYS_WAIT:
      wait(*(int *)(f->esp + 4));
      break;
    case SYS_CREATE:
      create(*(char **)(f->esp + 4), *(unsigned *)(f->esp + 8));
      break;
    case SYS_REMOVE:
      remove(*(char **)(f->esp + 4));
      break;
    case SYS_OPEN:
      open(*(char **)(f->esp + 4));
      break;
    case SYS_FILESIZE:
      filesize(*(int *)(f->esp + 4));
      break;
    case SYS_READ:
      read(*(int *)(f->esp + 4), *(void **)(f->esp + 8), *(unsigned *)(f->esp + 12));
      break;
    case SYS_WRITE:
      write(*(int *)(f->esp + 4), *(void **)(f->esp + 8), *(unsigned *)(f->esp + 12));
      break;
    case SYS_SEEK:
      seek(*(int *)(f->esp + 4), *(unsigned *)(f->esp + 8));
      break;
    case SYS_TELL:
      tell(*(int *)(f->esp + 4));
      break;
    case SYS_CLOSE:
      close(*(int *)(f->esp + 4));
      break;
  }
}

/*
Terminates Pintos by calling shutdown_power_off() (declared in devices/shutdown.h).
This should be seldom used, because you lose some information about possible deadlock situations, etc.
*/
static void halt()
{
 shutdown_power_off();
}
/*
Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status that will be returned. 
Conventionally, a status of 0 indicates success and nonzero values indicate errors.
*/
static void exit(int status)
{
  struct thread *cur = thread_current();
  // set child status in child struct

  cur->exit_status = status;


  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();

}

/*
Runs the executable whose name is given in cmd_line, passing any given arguments, and returns the new process's program id (pid).
Must return pid -1, which otherwise should not be a valid pid, if the program cannot load or run for any reason. 
Thus, the parent process cannot return from the exec until it knows whether the child process successfully loaded its executable.
You must use appropriate synchronization to ensure this.
*/
static int exec(const char *cmd_line)
{
  printf("exec: %s\n", cmd_line);
  // execute the command line
  int pid = process_execute(cmd_line);
  return pid;

}

/*
Waits for a child process pid and retrieves the child's exit status.
If pid is still alive, waits until it terminates. 
Then, returns the status that pid passed to exit. 
If pid did not call exit(), but was terminated by the kernel (e.g. killed due to an exception), wait(pid) must return -1. 
It is perfectly legal for a parent process to wait for child processes that have already terminated by the time the parent calls wait,
but the kernel must still allow the parent to retrieve its child's exit status, or learn that the child was terminated by the kernel.

wait must fail and return -1 immediately if any of the following conditions is true:

pid does not refer to a direct child of the calling process. 
pid is a direct child of the calling process if and only if the calling process received pid as a return value from a successful call to exec.
Note that children are not inherited: if A spawns child B and B spawns child process C, then A cannot wait for C, even if B is dead. 
A call to wait(C) by process A must fail. 
Similarly, orphaned processes are not assigned to a new parent if their parent process exits before they do.

The process that calls wait has already called wait on pid. 
That is, a process may wait for any given child at most once.
Processes may spawn any number of children, wait for them in any order, and may even exit without having waited for some or all of their children. 
Your design should consider all the ways in which waits can occur. All of a process's resources, including its struct thread, must be freed whether its parent ever waits for it or not, and regardless of whether the child exits before or after its parent.

You must ensure that Pintos does not terminate until the initial process exits.
The supplied Pintos code tries to do this by calling process_wait() (in userprog/process.c) from main() (in threads/init.c).
We suggest that you implement process_wait() according to the comment at the top of the function and then implement the wait system call in terms of process_wait().

Implementing this system call requires considerably more work than any of the rest.
*/
static int wait(int pid){
  
    // wait for the process to finish
    return process_wait(pid);
}
/*
Creates a new file called file initially initial_size bytes in size. 
Returns true if successful, false otherwise. 
Creating a new file does not open it: opening the new file is a separate operation which would require a open system call.
*/
static bool create (const char *file, unsigned initial_size){

  // create the file
  return filesys_create(file, initial_size);

}
/*
Deletes the file called file. 
Returns true if successful, false otherwise. 
A file may be removed regardless of whether it is open or closed, and removing an open file does not close it. 
See Removing an Open File, for details.
*/
static bool remove (const char *file)
{
  // remove the file
  return filesys_remove(file);

}

/*
Opens the file called file. 
Returns a nonnegative integer handle called a "file descriptor" (fd), or -1 if the file could not be opened.
File descriptors numbered 0 and 1 are reserved for the console: fd 0 (STDIN_FILENO) is standard input, 
fd 1 (STDOUT_FILENO) is standard output. 
The open system call will never return either of these file descriptors, 
which are valid as system call arguments only as explicitly described below.

Each process has an independent set of file descriptors. File descriptors are not inherited by child processes.

When a single file is opened more than once, whether by a single process or different processes, 
each open returns a new file descriptor. 
Different file descriptors for a single file are closed independently in separate calls to close and they do not share a file position.
*/
static int open (const char *file)
{
  return filesys_open(file);

}

/*
Returns the size, in bytes, of the file open as fd.
*/
static int filesize (int fd)
{

  // get the file size
  return file_length(fd);

}
/*
Reads size bytes from the file open as fd into buffer. 
Returns the number of bytes actually read (0 at end of file), or -1 if the file could not be read 
(due to a condition other than end of file). 
Fd 0 reads from the keyboard using input_getc().
*/
static int read (int fd, void *buffer, unsigned size)
{
  
    // read from the file
    return file_read(fd, buffer, size);
}
/*
Writes size bytes from buffer to the open file fd. 
Returns the number of bytes actually written, which may be less than size if some bytes could not be written.

Writing past end-of-file would normally extend the file, but file growth is not implemented by the basic file system. 
The expected behavior is to write as many bytes as possible up to end-of-file and return the actual number written,
or 0 if no bytes could be written at all.

Fd 1 writes to the console. Your code to write to the console should write all of buffer in one call to putbuf(), 
at least as long as size is not bigger than a few hundred bytes. 
(It is reasonable to break up larger buffers.) 
Otherwise, lines of text output by different processes may end up interleaved on the console, 
confusing both human readers and our grading scripts.
*/
static int write (int fd, const void *buffer, unsigned size)
{
  // write to the file
  putbuf(buffer, size);
  return size;
  // return file_write(fd, buffer, size);

}
/*
Changes the next byte to be read or written in open file fd to position, 
expressed in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.)

A seek past the current end of a file is not an error. A later read obtains 0 bytes, indicating end of file. 
A later write extends the file, filling any unwritten gap with zeros. 
(However, in Pintos files have a fixed length until project 4 is complete, so writes past end of file will return an error.) 
These semantics are implemented in the file system and do not require any special effort in system call implementation.
*/
static void seek (int fd, unsigned position)
{
  // set the file position
  file_seek(fd, position);
}
/*
Returns the position of the next byte to be read or written in open file fd, expressed in bytes from the beginning of the file.
*/
static unsigned tell (int fd){

  // get the file position
  return file_tell(fd);
}
/*
Closes file descriptor fd. 
Exiting or terminating a process implicitly closes all its open file descriptors, as if by calling this function for each one.
*/
static void close (int fd)
{
  // close the file
  file_close(fd);
}

