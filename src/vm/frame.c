#include "frame.h"
#include "vm/page.h"
#include <stdio.h>
#include "threads/init.h"
#include "threads/palloc.h"

#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"


/* frame table (list of frame table entries)*/
static struct list frame_table;

/*frame table lock*/
static struct spinlock frame_table_lock;
struct list_elem* clock_hand;

void setup_frame_table(void) {
  /* init frame table and lock */
  list_init(&frame_table);
  spinlock_init(&frame_table_lock);
}

struct frame_table_entry* get_frame(void) {
  return get_multiple_frames(1);
}

struct frame_table_entry* get_multiple_frames(int num_frames) {
  struct frame_table_entry* new_frame = NULL;
  // attempt to get a free page from the page directory using the virtual addr 
  // of the provided page
  void* frame_addr = palloc_get_multiple(PAL_USER | PAL_ZERO, num_frames);
  if (frame_addr != NULL) {
    // TODO: create frame for EACH physical address
    return addr_to_frame(frame_addr);
  } else 
  {
    spinlock_acquire(&frame_table_lock);

    // evict a frame and return the evicted frame
    new_frame = evict();

    // if the bit is dirty, write to swap
    if(pagedir_is_dirty(new_frame->owner->pagedir, new_frame->page)){
      // write to swap
      new_frame->resident->index = swap_out_page(new_frame->page);
      new_frame->resident->location = IN_SWAP;
      
    }

    new_frame->page = NULL;
    new_frame->owner = thread_current();
    spinlock_release(&frame_table_lock);

    return new_frame;
  }
}


/* create and return frame_table_entry struct for provided physical */
struct frame_table_entry* addr_to_frame(void* frame_addr){
  struct frame_table_entry* new_frame = NULL;

    // TODO: add frames for each palloc-ed page
    // if we allocated a free page, create a new frame table entry
    new_frame = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));

    new_frame->physical_addr = frame_addr;
    new_frame->owner = thread_current();
    // no page is associated with this frame initially.
    new_frame->resident = NULL;

    spinlock_acquire(&frame_table_lock);
    list_push_back(&frame_table, &new_frame->elem);
    spinlock_release(&frame_table_lock);

    if (new_frame == NULL) {
      // failed to allocate memory for frame table entry
      palloc_free_page(frame_addr);
      return NULL;
    }
    return new_frame;
  }

/* free frame and remove from frame table*/
void free_frame(struct frame_table_entry* frame){
  // remove frame from the list
  spinlock_acquire(&frame_table_lock);
  list_remove(&frame->elem);
  spinlock_release(&frame_table_lock);
  // free physical page
  palloc_free_page(frame->physical_addr);
  // free frame pointer
  free(frame);
}

// helper function for eviction
// TEMP: returns addr of first element in frame table
// TODO: Add a fair eviction algorithm, send data to swap partition
// Use a clock algorithm to find the next frame to evict
struct frame_table_entry* evict(void) {

  struct frame_table_entry* frame = NULL;

  while(1){
    if(clock_hand == NULL){
      clock_hand = list_begin(&frame_table);
    }
    else{
      clock_hand = list_next(clock_hand);
    }

    if(clock_hand == list_end(&frame_table)){
      clock_hand = list_begin(&frame_table);
    }

    frame = list_entry(clock_hand, struct frame_table_entry, elem);
    if(pagedir_is_accessed(frame->owner->pagedir, frame->page))
    {
      pagedir_set_accessed(frame->owner->pagedir, frame->page, false);
    }
    else{
      break;
    }
  }
  return frame;
  

  
}

/* assign relevent values so corresponding frames and pages can be co-found */
void match_frame_page(struct frame_table_entry* frame, struct supp_page_table_entry* page){
    frame->resident = page;
    page->frame = frame;
    // install_page (page->virtual_addr, frame->physical_addr, true);
}
