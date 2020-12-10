#ifndef MMAP_H
#define MMAP_H
#include <inttypes.h>
#include <list.h>
#include "threads/palloc.h"
#include "vm/page.h"


struct mmap_file {
	int map_id;
	int page_num;
	struct list_elem mmap_elem;
	struct spte* spte;
};

int add_mmap_file(struct spte* spte);
struct mmap_file* find_mmap_file(int mapid);
#endif /* vm/mmap.h */
