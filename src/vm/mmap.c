#include "vm/mmap.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

int add_mmap_file(struct file* file){
	int read_bytes = file_length(file);
	struct mmap_file* mmap_file = malloc(sizeof(struct mmap_file));
	struct thread* cur = thread_current();

	int page_num;
	if(mmap_file==NULL)
		return -1;
	if(read_bytes==0)
		return -1;
	if(read_bytes % PGSIZE == 0){
        page_num = read_bytes / PGSIZE;
	}
    else{
        page_num = read_bytes / PGSIZE + 1;
    }
	mmap_file->page_num = page_num;

	int i = 0;
	struct list_elem* e;
	for(e = list_begin(&cur->mmap_file_list); e != list_end(&cur->mmap_file_list); e = list_next(e)){
		struct mmap_file *target = list_entry (e, struct mmap_file, mmap_elem);
		if(target->map_id != i){
			break;		
		}
		i++;
	}
	mmap_file->map_id = i;
	list_push_back(&thread_current()->mmap_file_list, &mmap_file->mmap_elem);
	return i;
}