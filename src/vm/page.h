#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <list.h>
#include <stdint.h>
#include <bitmap.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
enum page_status {
  IN_SWAP,
  NOT_IN_SWAP
};

// 8MB stack size
#define MAX_STACK_SIZE 8 * 1024 * 1024

struct supp_page_table_entry {
  void *virtual_addr;
  void *frame; // null if not resident, points to frame table entry
  struct thread *parent;
  struct list_elem elem;
  int fd;
  // TODO: file mapping stuff
  // TODO: add swap table information?
  int swap_index;
  enum page_status status;
  // lock stored in thread struct
};



// TODO: Implement
// function to set up page table, use lookup_page() to populate list of free pages
// void setup_supp_page_table();

// add new entry to page table
struct supp_page_table_entry* add_supp_page_entry(struct list* supp_page_table);
void free_supp_entry(struct supp_page_table_entry* supp_entry, mapid_t fd);
#endif
