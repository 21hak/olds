#ifndef MMAP_H
#define MMAP_H
#include <list.h>
#include "filesys/file.h"

struct mmap_file {
	struct list_elem mmap_elem;
	int map_id;
	// struct file*file;
	int page_num;
};
int add_mmap_file(struct file* file);

#endif /* vm/mmap.h */
