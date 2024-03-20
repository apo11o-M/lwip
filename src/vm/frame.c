#include "vm/frame.h"
#include "threads/init.h"
#include "threads/palloc.h"

struct frame_table_entry* get_frame(void){
    return get_multiple_frames(1);
}

struct frame_table_entry* get_multiple_frames(int num_frames){
    // attempt to get a free page from the page directory using the virtual addr of the provided page
    void* frame_addr = palloc_get_multiple(PAL_USER | PAL_ZERO, num_frames);
    // if no page exists
    if (frame_addr == NULL){
    // start eviction
        return evict();
    }
    else{
    // find frame table entry that corresponds to the free frame
    struct frame_table_entry* free_frame = NULL;
    struct list_elem *e;
    spinlock_acquire(&frame_table_lock);
    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = e->next)
    {
        struct frame_table_entry *curr = list_entry(e, struct frame_table_entry, elem);
        void *curr_frame_addr = curr->physical_addr;
        if (curr_frame_addr == frame_addr){
        free_frame = curr;
        break;
        }
    }
    spinlock_release(&frame_table_lock);
    return free_frame;
    }
}


// helper function for eviction
// TEMP: returns addr of first element in frame table
// TODO: Add a fair eviction algorithm, send data to swap partition
struct frame_table_entry* evict(){
    struct frame_table_entry* free_frame = NULL;
    spinlock_acquire(&frame_table_lock);
    free_frame = list_entry(list_begin(&frame_table), struct frame_table_entry, elem);
    spinlock_release(&frame_table_lock);
    return free_frame;
}
