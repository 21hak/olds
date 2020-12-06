#ifndef FRAME_H
#define FRAME_H
#include <inttypes.h>
#include <list.h>
#include "threads/palloc.h"
#include "vm/page.h"

struct frame_table_entry {
	uint8_t * frame_number;
	struct list_elem frame_elem;
	struct spte* mapped_page;
	int accessed_bit;

};

void frame_table_init(void);
struct frame_table_entry* allocate_frame(enum palloc_flags flag);
void deallocate_frame(uint8_t *kpage);
struct thread* find_thread(int tid);
struct frame_table_entry* select_victim();
void evict();

#endif /* vm/frame.h */
