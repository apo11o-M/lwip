#include "page.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include <list.h>
#include <stdio.h>

struct supp_page_table_entry* add_supp_page_entry(struct list* supp_page_table){
    struct supp_page_table_entry* new_entry = (struct supp_page_table_entry*) malloc(sizeof(struct supp_page_table_entry));    
    list_push_back(supp_page_table, &new_entry->elem);
    return new_entry;
}