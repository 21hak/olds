#ifndef PAGE_H
#define PAGE_H
#include <list.h>
#include <inttypes.h>
#include "threads/thread.h"
#include "filesys/file.h"


struct spte {
  uint8_t* page_number;
  uint8_t* frame_number;
  struct file* related_file;
  int offset;
  int read_bytes;
  int zero_bytes;
  bool writable;
  struct list_elem spt_elem;
};

struct spte* find_page(uint8_t* number);
#endif /* vm/page.h */
