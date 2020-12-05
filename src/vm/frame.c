#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

struct list frame_table;

void frame_table_init(){
	list_init(&frame_table);
}

uint8_t* allocate_frame(enum palloc_flags flag){
	uint8_t *kpage = palloc_get_page(flag);
	struct frame_table_entry* frame = malloc(sizeof(struct frame_table_entry));
	frame->frame_number = kpage;
	list_push_back(&frame_table, &frame->frame_elem);
	return kpage; 
}

void deallocate_frame(uint8_t *kpage){
	struct list_elem* e;
	for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
		struct frame_table_entry *target = list_entry (e, struct frame_table_entry, frame_elem);
		if(target->frame_number == kpage){
			list_remove(e);
			palloc_free_page(kpage);
			break;		
		}
	}
	// palloc_free_page(kpage);
}


// for (e = list_begin(list); e != list_end(list); e = list_next(e))
//      {
//        ...do something with e...
//      }