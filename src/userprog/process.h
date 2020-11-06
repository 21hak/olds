#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include <list.h>

struct dead_child{
	bool is_waiting;
	int tid;
	int exit_status;
	int load_status;
	struct list_elem dead_elem;

};

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
