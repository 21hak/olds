#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/interrupt.h"

void syscall_init (void);
void sys_exit(struct intr_frame *f);
bool check_valid(const void *vaddr);

#endif /* userprog/syscall.h */
