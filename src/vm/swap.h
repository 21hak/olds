#ifndef SWAP_H
#define SWAP_H

#include "threads/thread.h"

struct swap_sector {
	uint8_t *tag;
	bool is_avail;
};
bool is_swap(struct spte* victim);
void init_swap(void);
void swap_write(struct spte* victim);
void swap_read(struct spte* target, uint8_t *kpage);
#endif /* vm/swap.h */
