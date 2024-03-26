#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include <list.h>
#include <stdio.h>
#include <string.h>

struct supp_page_table_entry* add_supp_page_entry(struct list* supp_page_table){
    struct supp_page_table_entry* new_entry = (struct supp_page_table_entry*) malloc(sizeof(struct supp_page_table_entry));    
    list_push_back(supp_page_table, &new_entry->elem);
    return new_entry;
}

void free_supp_entry(struct supp_page_table_entry* supp_entry, mapid_t fd){
    struct frame_table_entry* curr_frame = (struct frame_table_entry*)supp_entry->frame;
    list_remove(&supp_entry->elem);
    /* if page was written to, write back to data */
    if(pagedir_is_dirty(thread_current()->pagedir, supp_entry->virtual_addr)){
        printf("writing dirty page\n");
        file_write (thread_current()->file_descriptors[fd], supp_entry->virtual_addr, PGSIZE);
    }
    pagedir_clear_page(thread_current()->pagedir, supp_entry->virtual_addr);
    free(supp_entry);
}