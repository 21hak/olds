#include "vm/swap.h"
#include "vm/page.h"
#include "devices/block.h"
#include "threads/thread.h"

struct swap_sector* swap_table[8192];

void swap_table_init(){
	for(int i=0; i<8192; i++){
		swap_table[i]->is_avail = true;
	}
}

bool is_swap(struct frame_table_entry* frame){
	uint8_t* page_number = get_page_from_frame(frame);
	struct thread* thread = find_thread(frame->thread_id);
	return pagedir_is_dirty(thread->pagedir, page_number);

}

void swap_write(uint8_t* page_number, uint8_t* frame_number){
	int pos = 0;
	struct spte* page = find_page(page_number);
	page->related_file = NULL;
	for(;pos<8192; pos+=8){
		if(swap_table[pos]->is_avail){
			break;
		}
	}
	for(int i=0; i<8; i++){
		int offset = 512 * i;
		swap_table[pos+i]->page_number = page_number;
		swap_table[pos+i]->is_avail = false;
		swap_table[pos+i]->thread_id = thread_tid();
		block_write(block_get_role(BLOCK_SWAP), pos+i, frame_number);
	}

}

void swap_read(uint8_t* page_number, uint8_t* frame_number){
	int pos = 0;
	for(;pos<8192; pos+=8){
		if(swap_table[pos]->page_number == page_number){
			break;
		}
	}
	for(int i=0; i<8; i++){
		int offset = 512 * i;
		swap_table[pos+i]->is_avail = true;
		block_read(block_get_role(BLOCK_SWAP), pos+i, frame_number);
	}
}

void clear_swap(int tid){
	for(int i=0;i<8192; i++){
		if(swap_table[i]->is_avail && swap_table[i]->thread_id==tid){
			swap_table[i]->is_avail = true;
		}
	}
}