#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include <stdlib.h>

struct list frame_table;
extern struct list* pall_list;
struct frame_table_entry* clock_pointer;
struct lock frame_table_lock;

void frame_table_init(){
	list_init(&frame_table);
	lock_init(&frame_table_lock);
}

struct frame_table_entry* allocate_frame(enum palloc_flags flag){
	uint8_t *kpage = palloc_get_page(flag);
	struct frame_table_entry* frame;

	if(kpage!=NULL){
		frame = malloc(sizeof(struct frame_table_entry));
		frame->is_pinned=1;

		frame->frame_number = kpage;
		frame->accessed_bit = 1;
		// lock_acquire(&frame_table_lock);	
		// list_push_back(&frame_table, &frame->frame_elem);
		// lock_release(&frame_table_lock);

	}
	else{
		evict();
		uint8_t *kpage = palloc_get_page(flag);
		frame = malloc(sizeof(struct frame_table_entry));
		frame->is_pinned=1;
		frame->frame_number = kpage;
		frame->accessed_bit = 1;
		// lock_acquire(&frame_table_lock);	
		// list_push_back(&frame_table, &frame->frame_elem);
		// lock_release(&frame_table_lock);
	}
		
	return frame; 
}

void deallocate_frame(uint8_t *kpage){
		
	struct list_elem* e;
	lock_acquire(&frame_table_lock);	
	for(e = list_begin(&frame_table); e != list_end(&frame_table); e = e){
		struct frame_table_entry *target = list_entry (e, struct frame_table_entry, frame_elem);
		if(target->frame_number == kpage){
			e = list_remove(e);
			palloc_free_page(kpage);
			break;		
		}
		else {
			e = list_next(e);
		}
	}
	lock_release(&frame_table_lock);
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
	lock_acquire(&frame_table_lock);
	// timer_sleep(50);
	struct frame_table_entry* victim = select_victim();
	lock_release(&frame_table_lock);

	// while(victim->mapped_page->page_number>(uint8_t*)0x10000000)
	// lock_acquire(&frame_table_lock);
	// while(victim->mapped_page->page_number > (uint8_t*)0x10000000){
	// 	victim = select_victim();
	// }
	// lock_release(&frame_table_lock);
	struct thread* thread = find_thread(victim->mapped_page->thread_id);
	if(is_swap(victim) == 0){
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

