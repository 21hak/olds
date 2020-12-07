#ifndef SWAP_H
#define SWAP_H
#include <inttypes.h>
#include "threads/thread.h"


struct swap_sector {
	uint8_t* page_number;
	bool is_avail;
	int thread_id;
};
void swap_table_init(void);
void swap_write(uint8_t* page_number, uint8_t* frame_number);
void swap_read(uint8_t* page_number, uint8_t* frame_number);
int is_swap(struct frame_table_entry* frame);

#endif /* vm/swap.h */
