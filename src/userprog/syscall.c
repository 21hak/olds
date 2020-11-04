#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include <console.h>


static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // thread_exit ();
  // printf("%p\n", (uint32_t*)(f->esp));
  switch(*(uint32_t*)(f->esp)){
  	case SYS_HALT:
  		halt();
      	break;
    case SYS_EXIT:
      	exit(f);
      	break;
    case SYS_EXEC:
      	exec(f);
      	break;
    case SYS_WAIT:
      	wait(f);
      	break;
    case SYS_CREATE:
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
    	write(f);
      	break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
  }
}

void check_valid(const void *vaddr){
	if(!is_user_vaddr(vaddr)){
		// do something
	} else if(!pagedir_get_page(thread_current()->pagedir, vaddr)){
		// do something
	}

}

void halt (void){
	shutdown_power_off();
}

void exit(struct intr_frame *f){
	check_valid(f->esp + 4);
	int status = (int)*(uint32_t *)(f->esp +4);
	printf("%s: exit(%d)\n", thread_name(), status);
	thread_current()->exit_status = status;
	thread_exit();
} 

void exec(struct intr_frame *f){
	int tid;
	check_valid(f->esp +4);
	char* file = *(char*)(f->esp + 4);
	tid = process_execute(file);
	f->eax = (uint32_t)tid;
}

void wait(struct intr_frame *f){
	printf("wait\n");
	check_valid(f->esp +4);
	int pid = *(int*)(f->esp + 4);
	f->eax = (uint32_t)process_wait(pid);
}


void write(struct intr_frame *f){
	check_valid(f->esp+4);
	check_valid(f->esp+8);
	check_valid(f->esp+12);
	int fd = (int)*(uint32_t *)(f->esp+4);
	char *buffer = (void*)*(uint32_t *)(f->esp+8);
	unsigned size = (unsigned)*(uint32_t *)(f->esp+12);
	
	if (fd == 1) {
    	putbuf(buffer, size);
   		f->eax = size;
  	}
}	