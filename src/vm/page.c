#include "vm/page.h"
#include "threads/vaddr.h"

struct spte* find_page(uint8_t* number){
	struct list_elem* e;
	struct thread* cur = thread_current();
	for(e = list_begin(&cur->spt); e != list_end(&cur->spt); e = list_next(e)){
		struct spte *target = list_entry (e, struct spte, spt_elem);
		if(target->page_number == pg_round_down(number)){
			return target;		
		}
	}
	return NULL;
}