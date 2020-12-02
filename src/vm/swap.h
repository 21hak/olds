#ifndef SWAP_H
#define SWAP_H

#include "threads/thread.h"

struct swap_sector {
	uint8_t *tag;
	bool is_avil;
}
bool is_swap(struct spte* victim);
void init_swap(void);
#endif /* vm/swap.h */
