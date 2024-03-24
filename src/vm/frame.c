#include "frame.h"
#include "vm/page.h"
#include <stdio.h>
#include "threads/init.h"
#include "threads/palloc.h"

#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"


void setup_frame_table(void) {
  /* init frame table and lock */
  list_init(&frame_table);
  spinlock_init(&frame_table_lock);
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

struct frame_table_entry* get_frame(void) {
  return get_multiple_frames(1);
}

struct frame_table_entry* get_multiple_frames(int num_frames) {
  struct frame_table_entry* new_frame = NULL;
  void* frame_addr;

  // attempt to get a free page from the page directory using the virtual addr 
  // of the provided page
  void* frame_addr = palloc_get_multiple(PAL_USER | PAL_ZERO, num_frames);
  if (frame_addr != NULL) {
    // TODO: create frame for EACH physical address
    return addr_to_frame(frame_addr);
  } else {
    // failed to allocate a page, start eviction
    return evict();
  }
}


/* create and return frame_table_entry struct for provided physical */
struct frame_table_entry* addr_to_frame(void* frame_addr){
  struct frame_table_entry* new_frame = NULL;

    // TODO: add frames for each palloc-ed page
    // if we allocated a free page, create a new frame table entry
    new_frame = (struct frame_table_entry*)malloc(sizeof(struct frame_table_entry));

    new_frame->physical_addr = frame_addr;
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
  }
  else {
    // failed to allocate a page, start eviction
    spinlock_acquire(&frame_table_lock); 
    new_frame = evict();
    
    struct thread *t = thread_current();


    void *addr = new_frame->resident->virtual_addr;
    void * kadd = pagedir_get_page(t->pagedir, addr);

    // find the page that is associated with the frame by iterating through the supplemental page table
    struct list_elem* e;
    struct supp_page_table_entry* curr;
    for (e = list_begin(&t->supp_page_table); e != list_end(&t->supp_page_table); e = list_next(e)) {
       curr = list_entry(e, struct supp_page_table_entry, elem);
      if (curr->virtual_addr == addr) {
        break;
      }
    }

    if (curr == NULL) {
      PANIC("Page not found in supplemental page table");
    }

    // write to swap partition
    curr->frame = new_frame;
    int swap_index = swap_out_page(kadd);
    curr->swap_index = swap_index;

    // update the page table entry
    curr->parent = t;
    curr->status = IN_SWAP;
    pagedir_clear_page(t->pagedir, addr);
    palloc_free_multiple(new_frame->physical_addr, num_frames);

    spinlock_release(&frame_table_lock);


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
  struct list_elem* e;

   
  while(frame == NULL){
    for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
      struct frame_table_entry* curr = list_entry(e, struct frame_table_entry, elem);
      if(pagedir_is_accessed(curr->resident->parent->pagedir, curr->resident->virtual_addr) == true){
        frame = curr;
        return frame;
      }
      else {
        pagedir_set_accessed(curr->resident->parent->pagedir, curr->resident->virtual_addr, false);
      }
    }
  }

  return NULL;
  
}

/* assign relevent values so corresponding frames and pages can be co-found */
void match_frame_page(struct frame_table_entry* frame, struct supp_page_table_entry* page){
    frame->resident = page;
    page->frame = frame;
    // install_page (page->virtual_addr, frame->physical_addr, true);
}