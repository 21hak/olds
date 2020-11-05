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
#include "filesys/filesys.h"
#include "devices/input.h"



static void syscall_handler (struct intr_frame *);
void sys_halt (void);
void sys_exit(struct intr_frame *f);
void sys_exec(struct intr_frame *f);
void sys_wait(struct intr_frame *f);
void sys_write(struct intr_frame *f);
void sys_create(struct intr_frame *f);
void sys_remove(struct intr_frame *f);
void sys_remove(struct intr_frame *f);
void sys_open(struct intr_frame *f);
void sys_close(struct intr_frame *f);
void sys_read(struct intr_frame *f);
void sys_seek(struct intr_frame *f);
void sys_tell(struct intr_frame *f);
void file_size(struct intr_frame *f);
bool check_valid(const void *vaddr);



struct semaphore read_sema;
struct semaphore write_sema;
int read_count;
extern struct list* pall_list;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  sema_init(&read_sema, 1);
  sema_init(&write_sema, 1);
  read_count = 0;
}

bool check_valid(const void *vaddr){
	for(int i = 0; i < 4; i++){
		if(vaddr+i==NULL || !is_user_vaddr(vaddr+i) || vaddr+i <= 0x8048000){
			return false;
		}
		void * ptr = pagedir_get_page(thread_current()->pagedir, vaddr+i); 
		if(!ptr){
			return false;
		}	
	}
	return true;

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{	
	if(check_valid(f->esp)){
		switch(*(uint32_t*)(f->esp)){
		case SYS_HALT:
			sys_halt();
		  	break;
		case SYS_EXIT:
		  	sys_exit(f);
		  	break;
		case SYS_EXEC:
		  	sys_exec(f);
		  	break;
		case SYS_WAIT:
		  	sys_wait(f);
		  	break;
		case SYS_CREATE:
			sys_create(f);
		  	break;
		case SYS_REMOVE:
			sys_remove(f);
		  	break;
		case SYS_OPEN:
			sys_open(f);
	  		break;
		case SYS_FILESIZE:
		  	file_size(f);
		  	break;
		case SYS_READ:
			sys_read(f);
		 	break;
		case SYS_WRITE:
			sys_write(f);
		  	break;
		case SYS_SEEK:
			sys_seek(f);
		  	break;
		case SYS_TELL:
			sys_tell(f);
		  	break;
		case SYS_CLOSE:
			sys_close(f);
		  	break;	
		  }
	} else{
		sys_exit(f);
	}

}

// static int 
// get_user (const uint8_t *uaddr){
// 	int result;
// 	asm("movl $1f, %0; movzbl %1, %0; 1:": "=&a"(result) : "m"(*uaddr));
// 	return result;
// }



void sys_halt (void){
	shutdown_power_off();
}

void sys_exit(struct intr_frame *f){
	int exit_status = -1;
	if (check_valid(f->esp + 4)){
		exit_status = (int)*(uint32_t *)(f->esp +4);
		thread_current()->exit_status = exit_status;
	}
	printf("%s: exit(%d)\n", thread_name(), exit_status);
	thread_exit();
} 

void sys_exec(struct intr_frame *f){
	int tid;
	if (!check_valid(f->esp +4))
		sys_exit(f);
	char* file = (char*)*(uint32_t *)(f->esp + 4);
	if(!check_valid(file)){
		f->esp = NULL;
		sys_exit(f);
	}
	tid = process_execute(file);
	f->eax = (uint32_t)tid;
}

void sys_wait(struct intr_frame *f){
	if (!check_valid(f->esp +4))
		sys_exit(f);
	int pid = *(int*)(f->esp + 4);
	f->eax = (uint32_t)process_wait(pid);
}


void sys_write(struct intr_frame *f){
	if (!check_valid(f->esp+4))
		sys_exit(f);
	if (!check_valid(f->esp+8)){
		f->esp=NULL;
		sys_exit(f);
	}
	if (!check_valid(f->esp+12)){
		f->esp=NULL;
		sys_exit(f);
	}
	
	int fd = (int)*(uint32_t *)(f->esp+4);
	char *buffer = (void*)*(uint32_t *)(f->esp+8);
	unsigned size = (unsigned)*(uint32_t *)(f->esp+12);
	int write_bytes = 0;
	if(!check_valid(buffer)){
		f->esp = NULL;
		sys_exit(f);
	}
	if(fd>127 || fd==0){
		f->esp = NULL;
		sys_exit(f);
	}
	sema_down(&write_sema);
	if (fd == 1) {
    	putbuf(buffer, size);
   		f->eax = size;
  	} else {
  		if(thread_current()->open_file_list[fd]!=NULL){
			write_bytes = file_write(thread_current()->open_file_list[fd], (void*)buffer, size);
		}
		f->eax = write_bytes;
  	}
  	sema_up(&write_sema);
}	

void sys_create(struct intr_frame *f){
	bool rst;
	if (!check_valid(f->esp+4))
		sys_exit(f);
	if (!check_valid(f->esp+8)){
		f->esp = NULL;
		sys_exit(f);
	}
		
	char *name = (char*)*(uint32_t *)(f->esp+4);
	if(!check_valid(name)){
		f->esp = NULL;
		sys_exit(f);
	}else{
		unsigned initial_size = (unsigned)*(uint32_t *)(f->esp+8);
		rst = filesys_create (name, initial_size);
		f->eax = rst;	
	}	
}

void sys_remove(struct intr_frame *f){
	bool rst;
	char *name; 
	if (!check_valid(f->esp+4))
		sys_exit(f);
	name = (char*)*(uint32_t *)(f->esp+4);
	rst = filesys_remove (name);
	f->eax = rst;
}


void sys_open(struct intr_frame *f){
	int fd = -1;
	char* name;
	struct file* o_file;
	struct list_elem *e;

	if (!check_valid(f->esp+4))
		sys_exit(f);
	name = (char*)*(uint32_t *)(f->esp+4);
	if(!check_valid(name)){
		f->esp = NULL;
		sys_exit(f);
	}

	o_file = filesys_open (name);

	
	if(o_file){
		for (e = list_begin (pall_list); e != list_end (pall_list); e = list_next (e))
	    {
	      struct thread *t = list_entry (e, struct thread, allelem);
	      if(strcmp(name, t->name)==0){
	      	file_deny_write(o_file);
	      	break;
	      }
	    }
		for(fd = 2; fd<128 ; fd++){
			if(thread_current()->open_file_list[fd]==NULL){
				thread_current()->open_file_list[fd] = o_file;
				break;
			} 
		}
		if(fd==128){
			fd = -1;
		}
	} else {
		f->esp = NULL;
		sys_exit(f);
	}


	f->eax = fd;	
}

void sys_close(struct intr_frame *f){
	int fd;
	if (!check_valid(f->esp+4))
		sys_exit(f);
	fd = (int)*(uint32_t *)(f->esp+4);
	if(fd>127 || fd<2){
		f->esp = NULL;
		sys_exit(f);
	}
	if(thread_current()->open_file_list[fd]!=NULL){
		file_close(thread_current()->open_file_list[fd]);
		thread_current()->open_file_list[fd] = NULL;
	}
}

void sys_read(struct intr_frame *f){
	if (!check_valid(f->esp+4))
		sys_exit(f);
	if (!check_valid(f->esp+8)){
		f->esp = NULL;
		sys_exit(f);
	}
	if (!check_valid(f->esp+12)){
		f->esp = NULL;
		sys_exit(f);
	}
	
	int fd = (int)*(uint32_t *)(f->esp+4);
	char *buffer = (void*)*(uint32_t *)(f->esp+8);
	unsigned size = (unsigned)*(uint32_t *)(f->esp+12);
	int read_bytes = 0;
	if (!check_valid(buffer)){
		f->esp=NULL;
		sys_exit(f);
	}
	if(fd>127||fd==1){
		f->esp = NULL;
		sys_exit(f);
	}
	sema_down(&read_sema);
	read_count++;
	if(read_count==1){
		sema_down(&write_sema);
	}
	sema_up(&read_sema);
	if(fd == 0){
		for(int i = 0; i < (int)size; i++){
			*(buffer+i) = input_getc();
		}
		f->eax = size;
	} else {
		if(thread_current()->open_file_list[fd]!=NULL){
			read_bytes = file_read(thread_current()->open_file_list[fd], (void*)buffer, size);
		} else {
			read_bytes = -1;
		}
	 	f->eax = read_bytes;
	}
	sema_down(&read_sema);
	read_count--;
	if(read_count==0){
		sema_up(&write_sema);
	}
	sema_up(&read_sema);
}

void sys_seek(struct intr_frame *f){
	if (!check_valid(f->esp+4)){
		f->esp=NULL;
		sys_exit(f);
	}
	if (!check_valid(f->esp+8)){
		f->esp=NULL;
		sys_exit(f);
	}
	int fd = (int)*(uint32_t *)(f->esp+4);
	if(fd>127 || fd<2){
		f->esp = NULL;
		sys_exit(f);
	}
	unsigned new_pos = (unsigned)*(uint32_t *)(f->esp+8);
	if(thread_current()->open_file_list[fd] != NULL){
		file_seek(thread_current()->open_file_list[fd], new_pos);
	}
}

void sys_tell(struct intr_frame *f){
	int pos = 0;
	if (!check_valid(f->esp+4))
		sys_exit(f);
	int fd = (int)*(uint32_t *)(f->esp+4);
	if(fd>127 || fd<2){
		f->esp = NULL;
		sys_exit(f);
	}
	if(thread_current()->open_file_list[fd] != NULL){
		pos = file_tell(thread_current()->open_file_list[fd]);
		f->eax = pos;
	}
}

void file_size(struct intr_frame *f){
	int size = 0;
	if (!check_valid(f->esp+4))
		sys_exit(f);
	int fd = (int)*(uint32_t *)(f->esp+4);
	if(fd>127 || fd<2){
		f->esp = NULL;
		sys_exit(f);
	}
	if(thread_current()->open_file_list[fd] != NULL){
		size = file_length(thread_current()->open_file_list[fd]);
		f->eax = size;
	}
}
