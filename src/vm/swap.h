#ifndef SWAP_H
#define SWAP_H
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include "vm/frame.h"

struct swap_sector {
	uint8_t* page_number;
	bool is_avail;
	int thread_id;
};
void swap_table_init(void);
bool is_swap(struct frame_table_entry* frame);
void swap_write(uint8_t* page_number, uint8_t* frame_number);
void swap_read(uint8_t* page_number, uint8_t* frame_number);
void clear_swap(int tid);
#endif /* vm/swap.h */
