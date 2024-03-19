// gets a free frame and returns phys addr of free frame
// evicts if necessary
void* get_frame(void* v_addr);
// helper function for eviction
void evict();

// moves frame to swap partition
// panic if no slots available
void move_to_swap();
// moves frame from swap partition, frees slot
void move_from_swap();
