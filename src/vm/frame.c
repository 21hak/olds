#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "vm/frame.h"

struct list frame_table;
extern struct list* pall_list;
struct frame_table_entry* clock_pointer;

void frame_table_init(){
	list_init(&frame_table);
}

struct frame_table_entry* allocate_frame(enum palloc_flags flag){
	uint8_t *kpage = palloc_get_page(flag);
	struct frame_table_entry* frame;
	if(kpage!=NULL){
		frame = malloc(sizeof(struct frame_table_entry));
		frame->frame_number = kpage;
		frame->accessed_bit = 1;
		list_push_back(&frame_table, &frame->frame_elem);
	}
	else{
		evict();
		uint8_t *kpage = palloc_get_page(flag);
		frame = malloc(sizeof(struct frame_table_entry));
		frame->frame_number = kpage;
		frame->accessed_bit = 1;
		list_push_back(&frame_table, &frame->frame_elem);
	}

	return frame; 
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

struct thread* find_thread(int tid){
	struct list_elem* e;
	struct thread* thread;
	for(e = list_begin(pall_list); e != list_end(pall_list); e = list_next(e)){
		thread = list_entry(e, struct thread, allelem);
		if(thread->tid == tid){
			return thread;
		}
	}
	return NULL;
}

struct frame_table_entry* select_victim(){
	struct thread* target;
	struct frame_table_entry* frame;
	struct list_elem* e;
	// return list_entry(list_begin(&frame_table), struct frame_table_entry, frame_elem);
	if(clock_pointer==NULL){
		clock_pointer = list_entry(list_begin(&frame_table), struct frame_table_entry, frame_elem);
	}
	target = find_thread(clock_pointer->mapped_page->thread_id);

	while(pagedir_is_accessed(target->pagedir, clock_pointer->mapped_page->page_number)
	 // && (clock_pointer->accessed_bit == 1)
	 ){
		pagedir_set_accessed(target->pagedir, clock_pointer->mapped_page->page_number, false);
		// clock_pointer->accessed_bit = 0;
		if(clock_pointer == list_entry(list_rbegin(&frame_table), struct frame_table_entry, frame_elem)){
			clock_pointer = list_entry(list_begin(&frame_table), struct frame_table_entry, frame_elem);
		}
		else {
			clock_pointer = list_entry(list_next(&clock_pointer->frame_elem), struct frame_table_entry, frame_elem);
		}
		
		target = find_thread(clock_pointer->mapped_page->thread_id);

	}
	if(&clock_pointer->frame_elem == list_rbegin(&frame_table)){
		clock_pointer = list_entry(list_begin(&frame_table), struct frame_table_entry, frame_elem);
		return list_entry(list_rbegin(&frame_table), struct frame_table_entry, frame_elem);
	}
	else {
		clock_pointer = list_entry(list_next(&clock_pointer->frame_elem), struct frame_table_entry, frame_elem);
		return list_entry(list_prev(&clock_pointer->frame_elem), struct frame_table_entry, frame_elem);
	}
}

void evict() {
	struct frame_table_entry* victim = select_victim();
	struct thread* thread = find_thread(victim->mapped_page->thread_id);
	if(!is_swap(victim)){
		pagedir_clear_page(thread->pagedir, victim->mapped_page->page_number);
		deallocate_frame(victim->frame_number);
	}
	else{
		// swap
		swap_write(victim->mapped_page->page_number, victim->frame_number);
		pagedir_clear_page(thread->pagedir, victim->mapped_page->page_number);
		deallocate_frame(victim->frame_number);
	}
}

// for (e = list_begin(list); e != list_end(list); e = list_next(e))
//      {
//        ...do something with e...
//      }