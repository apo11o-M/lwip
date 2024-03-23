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
  struct frame_table_entry* new_frame = NULL;
  void* frame_addr;
   
  // attempt to get a free page from the page directory using the virtual addr 
  // of the provided page
  frame_addr = palloc_get_multiple(PAL_USER | PAL_ZERO, num_frames);

  if (frame_addr != NULL) {
    // if we allocated a free page, create a new frame table entry
    new_frame = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));

    new_frame->physical_addr = frame_addr;
    // no page is associated with this frame initially.
    new_frame->resident = NULL;

    spinlock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &new_frame->elem);
    spinlock_release(&frame_table_lock);

    if (new_frame == NULL){
      // failed to allocate memory for frame table entry
      palloc_free_multiple(frame_addr, num_frames);
      return NULL;
    }
  } else {
    // failed to allocate a page, start eviction
    new_frame = evict();
  }

  return new_frame;
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
