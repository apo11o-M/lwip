// gets a free frame and returns phys addr of free frame
// evicts if necessary
void* get_frame(void* v_addr);
// helper function for eviction
void evict();