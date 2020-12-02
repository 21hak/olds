#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "devices/block.h"

struct swap_sector* swap_array[8192];

bool is_swap(struct spte* victim){
	return pagedir_is_dirty(thread_current()->pagedir, victim->tag);
}

void init_swap(){
	for(int i = 0; i < 8192; i++){
		swap_array[i]-> is_avail = true;
	}
}

void swap_write(struct spte* victim){
	int pos = 0;
	for(; pos < 8192; pos++){
		if(swap_array[pos]->is_avail==true){
			break;
		}
	}
	for(int i = 0; i < 8; i++){
		int offset = 512*i;
		swap_array[pos+i]->is_avail = false;
		swap_array[pos+i]->tag = victim->tag;
		block_write(block_get_role(BLOCK_SWAP), pos+i,
		 pagedir_get_page(thread_current()->pagedir, victim->tag) + offset);
	}
	victim->file = NULL;
	pagedir_clear_page(thread_current()->pagedir, victim->tag);
}

void swap_read(struct spte* target, uint8_t *kpage){
	int pos = 0;
	for(; pos < 8192; pos++){
		if(swap_array[pos]->tag==target->tag){
			break;
		}
	}
	for(int i = 0; i < 8; i++){
		int offset = 512 * i;
		swap_array[pos+i]->is_avail = true;
		block_read(block_get_role(BLOCK_SWAP), pos+i, kpage + offset);
	}
}