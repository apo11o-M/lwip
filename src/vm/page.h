#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <list.h>
#include <stdint.h>
#include <bitmap.h>
#include "threads/synch.h"

struct supp_page_table_entry {
  void *virtual_addr;
  void *frame; // null if not resident, points to frame table entry
  struct thread *parent;
  struct list_elem elem;
  // TODO: file mapping stuff
  // TODO: add swap table information?
  // lock stored in thread struct
};



// TODO: Implement
// function to set up page table, use lookup_page() to populate list of free pages
// void setup_supp_page_table();

// add new entry to page table
struct supp_page_table_entry* add_supp_page_entry(struct list* supp_page_table);

#endif
