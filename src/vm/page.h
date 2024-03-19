#include <list.h>
#include <stdint.h>
#include <bitmap.h>

struct page_table_entry {
  void *virtual_addr;
  void *frame; // null if not resident
  struct thread *parent;
  struct list_elem elem;
  // TODO: file mapping stuff
  // lock stored in thread struct
};

struct frame_table_entry {
    void *physical_addr;
    struct page_table_entry *resident;
    struct list_elem elem;
};
struct spinlock frame_table_lock; // TODO: look at turning this into a per-frame lock

struct list frame_table;

struct bitmap swap_table;
struct spinlock swap_table_lock;
