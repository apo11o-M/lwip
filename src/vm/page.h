#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <list.h>
#include <stdint.h>
#include <bitmap.h>

struct supp_page_table_entry {
  void *virtual_addr;
  void *frame; // null if not resident, points to frame table entry
  struct thread *parent;
  struct list_elem elem;
  // TODO: file mapping stuff
  // TODO: add swap table information?
  // lock stored in thread struct
};


#endif