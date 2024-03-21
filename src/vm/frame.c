#include "vm/frame.h"
#include "threads/init.h"
#include "threads/palloc.h"
#include <stdio.h>
#include "threads/malloc.h"

void setup_frame_table(void){
    /* init frame table and lock */
    list_init(&frame_table);
    spinlock_init (&frame_table_lock);
    /* add frames to the supplemental page table while */
    // spinlock_acquire(&frame_table_lock);
    // void* frame_ptr = palloc_get_page(PAL_USER | PAL_ZERO);
    // while(frame_ptr){
    //     struct frame_table_entry* new_frame_table_entry = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));
    //     frame_ptr = palloc_get_page(PAL_USER | PAL_ZERO);   
    //     new_frame_table_entry->physical_addr = frame_ptr;
    // }
    // spinlock_release(&frame_table_lock);
}

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
    /*
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
    */
    /* create new frame table entry*/
    struct frame_table_entry* new_frame = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));
    new_frame->physical_addr = frame_addr;
    /* add new frame table entry to list*/
    spinlock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &new_frame->elem);
    spinlock_release(&frame_table_lock);
    return new_frame;
    }
}


// helper function for eviction
// TEMP: returns addr of first element in frame table
// TODO: Add a fair eviction algorithm, send data to swap partition
struct frame_table_entry* evict(void){
    PANIC("trying to evict");
    // struct frame_table_entry* free_frame = NULL;
    // spinlock_acquire(&frame_table_lock);
    // free_frame = list_entry(list_begin(&frame_table), struct frame_table_entry, elem);
    // spinlock_release(&frame_table_lock);
    // return free_frame;
}
