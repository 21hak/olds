#include "vm/swap.h"
#include "devices/block.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "devices/timer.h"

extern frame_table_lock;
extern frame_table;
struct lock swap_table_lock;

struct swap_sector swap_table[8192];

void swap_table_init(){
	lock_init(&swap_table_lock);
	for(int i=0; i<8192; i++){
		swap_table[i].is_avail = true;
	}
}

void swap_write(uint8_t* page_number, uint8_t* frame_number){
	
	lock_acquire(&swap_table_lock);
	int pos = 0;
	struct spte* page = find_page_from_frame(page_number);
	// lock_acquire(&frame_table_lock);
	
	page->related_file = NULL;

	for(;pos<8192; pos+=8){
		if(swap_table[pos].is_avail){
			break;
		}
	}
	for(int i=0; i<8; i++){
		int offset = 512 * i;
		swap_table[pos+i].page_number = page_number;
		swap_table[pos+i].is_avail = false;
		swap_table[pos+i].thread_id = page->thread_id;
		block_write(block_get_role(BLOCK_SWAP), pos + i, page_number + offset);
	}
	// lock_release(&frame_table_lock);
	lock_release(&swap_table_lock);

}


void swap_read(uint8_t* page_number, uint8_t* frame_number){
	lock_acquire(&swap_table_lock);
	int pos = 0;
	for(;pos<8192; pos+=8){
		if(swap_table[pos].page_number == page_number){
			break;
		}
	}
	if(pos==8192)
		return;
	for(int i=0; i<8; i++){
		int offset = 512 * i;
		swap_table[pos+i].is_avail = true;
		block_read(block_get_role(BLOCK_SWAP), pos+i, page_number +offset);
	}
	lock_release(&swap_table_lock);

}

int is_swap(struct frame_table_entry* frame){
	struct thread* thread = find_thread(frame->mapped_page->thread_id);
	if (pagedir_is_dirty(thread->pagedir, frame->mapped_page->page_number)){
		return 1;
	}
	else{
		return 0;
	}
	return 0;

}

void clear_swap_table(){
	lock_acquire(&swap_table_lock);
	for(int i=0; i<8192; i++){
		if(thread_tid()==swap_table[i].thread_id)
			swap_table[i].is_avail = true;
	}
	lock_release(&swap_table_lock);
}