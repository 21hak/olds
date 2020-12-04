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
struct thread* find_thread(int tid);
uint8_t* allocate_frame(enum palloc_flags flag);
void deallocate_frame(uint8_t *kpage);
struct frame_table_entry* select_victim(void);
uint8_t* get_page_from_frame(struct frame_table_entry* frame);
void evict(void);

#endif /* vm/frame.h */
