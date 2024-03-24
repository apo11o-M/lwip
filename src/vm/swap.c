#include "swap.h"
// panic
#include <debug.h>
struct block *swap_block;

struct bitmap* swap_table;

/*
Initializes the swap block and swap table
*/
void swap_init(void){
    swap_block = block_get_role(BLOCK_SWAP);
    
    if(swap_block == NULL){
        PANIC("Failed to get swap block");
    }

    swap_table = bitmap_create(block_size(swap_block) / 8);

}

/*
Swap out a page to the swap block
*/
int swap_out_page(void* frame){
    int swap_index = bitmap_scan_and_flip(swap_table, 0, 1, false);

    // if no free swap slots, return -1
    for(int i = 0; i < 8; i++){
        block_write(swap_block, swap_index * 8 + i, frame + i * BLOCK_SECTOR_SIZE);
    }

    return swap_index;
}

/*
Swap in a page from the swap block
*/
void swap_in_page(int swap_index, void* frame){

    // read 8 sectors from the swap block
    for(int i = 0; i < 8; i++){
        block_read(swap_block, swap_index * 8 + i, frame + i * BLOCK_SECTOR_SIZE);
    }

    bitmap_flip(swap_table, swap_index);
}

/*
Mark a swap index as free
*/
void swap_free_slot(int swap_index){
    bitmap_flip(swap_table, swap_index);
}

/*
Destroy the swap table 
*/
void swap_destroy(void){
    bitmap_destroy(swap_table);
}
