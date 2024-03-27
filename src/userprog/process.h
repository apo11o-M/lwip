#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/page.h"
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable);

bool load_segment (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int fd);
bool load_segment_mmap (struct file *file, off_t ofs, struct supp_page_table_entry *new_page_entry, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

#endif /* userprog/process.h */
