#include "vm/frame.h"
#include "userprog/pagedir.h"

struct spte* select_victim(){
	struct spte* clock_pointer = thread_current()->clock_pointer;
	if(clock_pointer == NULL){
		clock_pointer = list_head(&thread_current()->frame_table);
		// while(pagedir_is_accessed(thread_current()->pagedir, clock_pointer->tag)){
		// 	if(is_tail(&clock_pointer->frame_elem)){
		// 		clock_pointer = list_head(&thread_current()->frame_table);
		// 	}
		// 	clock_pointer = list_next(&clock_pointer->frame_elem);
		// }
	}

	while(pagedir_is_accessed(thread_current()->pagedir, clock_pointer->tag)){
		if(&clock_pointer->frame_elem == list_tail(&thread_current()->frame_table)){
			clock_pointer = list_head(&thread_current()->frame_table);
		}
		clock_pointer = list_next(&clock_pointer->frame_elem);
	}

	if(&clock_pointer->frame_elem == list_tail(&thread_current()->frame_table)){
		clock_pointer = list_head(&thread_current()->frame_table);
		return list_entry(list_tail(&thread_current()->frame_table), struct spte, frame_elem) ;
	} else {
		clock_pointer = list_next(&clock_pointer->frame_elem);
		return list_entry(list_prev(&clock_pointer->frame_elem), struct spte, frame_elem);
	}
}