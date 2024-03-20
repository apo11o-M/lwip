#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/vaddr.h"
#include <list.h>
// macro to verify page alignment
#define aligned(addr)  ((addr) % PGSIZE == 0)


struct frame_table_entry {
    void *physical_addr;
    struct page_table_entry *resident;
    struct list_elem elem;
};
static struct list frame_table;

// gets a free frame and returns frame_table_entry corresponding to the selected frame
// evicts if necessary
struct frame_table_entry* get_frame(void* v_addr);

// helper function for eviction
// returns frame_table entry
struct frame_table_entry* evict();

// moves frame to swap partition
// panic if no slots available
void move_to_swap();
// moves frame from swap partition, frees slot
void move_from_swap();

// run use frame table to manage file mappings with mmap
void frame_mmap();
void frame_unmmap();

#endif