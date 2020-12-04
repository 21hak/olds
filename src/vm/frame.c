#include "vm/frame.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"


struct list frame_table;
extern struct list* pall_list;

void frame_table_init(){
	list_init(&frame_table);
}

struct thread* find_thread(int tid){
	struct list_elem* e;
	struct thread* thread;
	for(e = list_begin(pall_list); e != list_end(pall_list); e = list_next(e)){
		thread = list_entry(e, struct thread, elem);
		if(thread->tid == tid){
			return thread;
		}
	}
	return NULL;
}

uint8_t* allocate_frame(enum palloc_flags flag){
	uint8_t *kpage = palloc_get_page(flag);
	if(kpage!=NULL){
		struct frame_table_entry* frame = malloc(sizeof(struct frame_table_entry));
		frame->frame_number = kpage;
		list_push_back(&frame_table, &frame->frame_elem);
	} else {
		evict();
		uint8_t *kpage = palloc_get_page(flag);
		struct frame_table_entry* frame = malloc(sizeof(struct frame_table_entry));
		frame->frame_number = kpage;
		list_push_back(&frame_table, &frame->frame_elem);
	}
	
	return kpage; 
}

void deallocate_frame(uint8_t *kpage){
	struct list_elem* e;
	for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
		struct frame_table_entry *target = list_entry (e, struct frame_table_entry, frame_elem);
		if(target->frame_number == kpage){
			list_remove(e);
			break;		
		}
	}
	palloc_free_page(kpage);
}

uint8_t* get_page_from_frame(struct frame_table_entry* frame){
	struct list_elem* e;
	struct thread* thread = find_thread(frame->thread_id);
	for(e = list_begin(&thread->spt); e!= list_end(&thread->spt); e = list_next(e)){
		struct spte * target = list_entry(e, struct spte, spt_elem);
		if(target->frame_number == frame->frame_number){
			return target->page_number;
		}
	}
	return NULL;
}


struct frame_table_entry* select_victim() {
	struct thread* cur = thread_current();
	struct frame_table_entry* clock_pointer = cur->clock_pointer;
	if(clock_pointer==NULL)
		clock_pointer = list_entry(list_head(&frame_table), struct frame_table_entry, frame_elem);
	while(pagedir_is_accessed(cur->pagedir, get_page_from_frame(clock_pointer))){
		pagedir_set_accessed(cur->pagedir, get_page_from_frame(clock_pointer), false);
		if(&clock_pointer->frame_elem == list_tail(&frame_table)){
			clock_pointer = list_entry(list_head(&frame_table), struct frame_table_entry, frame_elem);
		}
		else {
			clock_pointer = list_entry(list_next(&clock_pointer->frame_elem), struct frame_table_entry, frame_elem);
		}
	}
	if(&clock_pointer->frame_elem == list_tail(&frame_table)){
		clock_pointer = list_entry(list_head(&frame_table), struct frame_table_entry, frame_elem);
		return list_entry(list_tail(&frame_table), struct frame_table_entry, frame_elem);
	}
	else {
		clock_pointer = list_entry(list_next(&clock_pointer->frame_elem), struct frame_table_entry, frame_elem);
		return list_entry(list_prev(&clock_pointer->frame_elem), struct frame_table_entry, frame_elem);
	}

}

void evict() {
	struct frame_table_entry* victim = select_victim();
	struct thread* thread = find_thread(victim->thread_id);
	uint8_t* page_number = get_page_from_frame(victim);
	if(!is_swap(victim)){
		pagedir_clear_page(thread->pagedir, page_number);
		deallocate_frame(victim->frame_number);
	}
	else{
		// swap
		swap_write(page_number, victim->frame_number);
		pagedir_clear_page(thread->pagedir, page_number);
		deallocate_frame(victim->frame_number);
	}

}