#include "vm/frame.h"
#include "threads/palloc.h"

struct frame_table_entry* get_frame(void* v_addr){
    // attempt to get a free page from the page directory using the virtual addr of the provided page
    void* frame_addr = palloc_get_page(flags);
    // if no page exists
    if (frame_addr == NULL){
    // start eviction
        frame_addr = evict();
    }
    // update frame table and supplemental page table
    fram_table_entry->frame_addr = frame_addr;
    supplemental_page_table_entry->frame = frame_table_entry;
}


// helper function for eviction
// TEMP: returns addr of first element in frame table
// TODO: Add a fair eviction algorithm
void* evict(){
    return addr of first element frame table
}
