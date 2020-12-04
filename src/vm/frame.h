#ifndef FRAME_H
#define FRAME_H
#include <inttypes.h>
#include <list.h>
#include "threads/palloc.h"


struct frame_table_entry {
	uint8_t * frame_number;
	int thread_id;
	struct list_elem frame_elem;

};

void frame_table_init(void);
uint8_t* allocate_frame(enum palloc_flags flag);
void deallocate_frame(uint8_t *kpage);
#endif /* vm/frame.h */
