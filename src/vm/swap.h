#include "devices/block.h"
#include <bitmap.h>

void swap_init(void);
int swap_out_page(void* frame);
void swap_in_page(int swap_index, void* frame);
void swap_free_slot(int swap_index);
void swap_destroy(void);
