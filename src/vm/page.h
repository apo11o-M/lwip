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

struct frame_table_entry {
    void *physical_addr;
    struct page_table_entry *resident;
    struct list_elem elem;
};
struct spinlock frame_table_lock; // TODO: look at turning this into a per-frame lock

struct bitmap swap_table;
struct spinlock swap_table_lock;
