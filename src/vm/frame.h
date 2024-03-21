#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/vaddr.h"
#include <list.h>
#include "threads/synch.h"

// macro to verify page alignment
#define aligned(addr)  ((addr) % PGSIZE == 0)


struct frame_table_entry {
    void *physical_addr;
    struct supp_page_table_entry *resident;
    struct list_elem elem;
};
/* frame table (list of frame table entries)*/
static struct list frame_table;

/*frame table lock*/
static struct spinlock frame_table_lock; // TODO: look at turning this into a per-frame lock

// gets a free frame and returns frame_table_entry corresponding to the selected frame
// evicts if necessary
struct frame_table_entry* get_frame(void);
struct frame_table_entry* get_multiple_frames(int num_frames);


// setup frame table
// continuously pull from palloc_get_page until all possible frames are acquired
void setup_frame_table(void);

// helper function for eviction
// returns frame_table entry
struct frame_table_entry* evict(void);

// moves frame to swap partition
// panic if no slots available
void move_to_swap(void);
// moves frame from swap partition, frees slot
void move_from_swap(void);

// run use frame table to manage file mappings with mmap
void frame_mmap(void);
void frame_unmmap(void);

#endif