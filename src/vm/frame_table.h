#include "vaddr.h"
// macro to verify page alignment
#define aligned(addr)  ((addr) % PGSIZE == 0)
// gets a free frame and returns phys addr of free frame
// evicts if necessary
void* get_frame(void* v_addr);
// helper function for eviction
void* evict();

// moves frame to swap partition
// panic if no slots available
void move_to_swap();
// moves frame from swap partition, frees slot
void move_from_swap();

// run use frame table to manage file mappings with mmap
void frame_mmap();
void frame_unmmap();