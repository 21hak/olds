#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "threads/synch.h"

extern frame_table_lock;
extern frame_table;
extern pall_list;

struct spte* find_page(uint8_t* number){
	struct list_elem* e;
	struct thread* cur = thread_current();
	lock_acquire(&cur->spt_lock);

	for(e = list_begin(&cur->spt); e != list_end(&cur->spt); e = list_next(e)){
		struct spte *target = list_entry (e, struct spte, spt_elem);
		if(target->page_number == pg_round_down(number)){
			lock_release(&cur->spt_lock);
			return target;		
		}
	}
	lock_release(&cur->spt_lock);
	return NULL;
}

struct spte* find_page_from_frame(uint8_t* number){
	lock_acquire(&frame_table_lock);	
	struct list_elem* e;
	for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)){
		struct frame_table_entry *target = list_entry(e, struct frame_table_entry, frame_elem);
		if(target->mapped_page->page_number == pg_round_down(number)){
			lock_release(&frame_table_lock);
			return target->mapped_page;		
		}
	}
	lock_release(&frame_table_lock);
	return NULL;
} 

struct spte* find_page_from_spts(uint8_t* number){
	struct list_elem* e;
	struct list_elem* e2;
	for(e = list_begin(pall_list); e != list_end(pall_list); e = list_next(e)){
		struct thread* target = list_entry(e, struct thread, allelem);
		for(e2 = list_begin(&target->spt); e2 != list_end(&target->spt); e2 = list_next(e2)){
			struct spte *spte = list_entry (e2, struct spte, spt_elem);
			if(spte->page_number == pg_round_down(number)){
				return spte;		
			}
		}
	}
	return NULL;

}



void clear_spt(){
	struct thread* cur= thread_current();
	struct list_elem* e;
	lock_acquire(&cur->spt_lock);
	for(e = list_begin(&cur->spt); e != list_end(&cur->spt); e = list_next(e)){
		struct spte *target = list_entry (e, struct spte, spt_elem);
		pagedir_clear_page(cur->pagedir, target->page_number);
		deallocate_frame(target->frame_number);
		list_remove(e);
	}
	lock_release(&cur->spt_lock);
}
