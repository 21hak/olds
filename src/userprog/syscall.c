#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/stdio.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/mmap.h"
#include "devices/timer.h"

struct lock filesys_lock;
extern frame_table_lock;

static void syscall_handler(struct intr_frame *);

static void check_vaddr(const void *);

static void syscall_halt(void);
static pid_t syscall_exec(const char *);
static int syscall_wait(pid_t);
static bool syscall_create(const char *, unsigned);
static bool syscall_remove(const char *);
static int syscall_open(const char *);
static int syscall_filesize(int);
static int syscall_read(int, void *, unsigned);
static int syscall_write(int, const void *, unsigned);
static void syscall_seek(int, unsigned);
static unsigned syscall_tell(int);
static int syscall_mmap(int fd, void *addr);

/* Registers the system call interrupt handler. */
void syscall_init(void)
{
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&filesys_lock);
}

/* Pops the system call number and handles system call
   according to it. */
static void
syscall_handler(struct intr_frame *f)
{
    void *esp = f->esp;
    int syscall_num;

    check_vaddr(esp);
    check_vaddr(esp + sizeof(uintptr_t) - 1);
    syscall_num = *(int *)esp;
    thread_current()->esp = esp;

    switch (syscall_num)
    {
    case SYS_HALT:
    {
        syscall_halt();
        NOT_REACHED();
    }
    case SYS_EXIT:
    {
        int status;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        status = *(int *)(esp + sizeof(uintptr_t));

        syscall_exit(status);
        NOT_REACHED();
    }
    case SYS_EXEC:
    {
        char *cmd_line;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        cmd_line = *(char **)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_exec(cmd_line);
        break;
    }
    case SYS_WAIT:
    {
        pid_t pid;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        pid = *(pid_t *)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_wait(pid);
        break;
    }
    case SYS_CREATE:
    {
        char *file;
        unsigned initial_size;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 3 * sizeof(uintptr_t) - 1);
        file = *(char **)(esp + sizeof(uintptr_t));
        initial_size = *(unsigned *)(esp + 2 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_create(file, initial_size);
        break;
    }
    case SYS_REMOVE:
    {
        char *file;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        file = *(char **)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_remove(file);
        break;
    }
    case SYS_OPEN:
    {
        char *file;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        file = *(char **)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_open(file);
        break;
    }
    case SYS_FILESIZE:
    {
        int fd;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_filesize(fd);
        break;
    }
    case SYS_READ:
    {
        int fd;
        void *buffer;
        unsigned size;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 4 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        buffer = *(void **)(esp + 2 * sizeof(uintptr_t));
        size = *(unsigned *)(esp + 3 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_read(fd, buffer, size);
        break;
    }
    case SYS_WRITE:
    {
        int fd;
        void *buffer;
        unsigned size;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 4 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        buffer = *(void **)(esp + 2 * sizeof(uintptr_t));
        size = *(unsigned *)(esp + 3 * sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_write(fd, buffer, size);
        break;
    }
    case SYS_SEEK:
    {
        int fd;
        unsigned position;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 3 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        position = *(unsigned *)(esp + 2 * sizeof(uintptr_t));

        syscall_seek(fd, position);
        break;
    }
    case SYS_TELL:
    {
        int fd;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));

        f->eax = (uint32_t)syscall_tell(fd);
        break;
    }
    case SYS_CLOSE:
    {
        int fd;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));

        syscall_close(fd);
        break;
    }
    case SYS_MMAP:
    {
        int fd;
        void* addr;

        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 3 * sizeof(uintptr_t) - 1);
        fd = *(int *)(esp + sizeof(uintptr_t));
        addr = *(void **)(esp + 2 * sizeof(uintptr_t));
        f->eax = syscall_mmap(fd, addr);
        break;
    }
    case SYS_MUNMAP:
    {
        int mapid;
        check_vaddr(esp + sizeof(uintptr_t));
        check_vaddr(esp + 2 * sizeof(uintptr_t) - 1);
        mapid = *(int *)(esp + sizeof(uintptr_t));
        syscall_munmap(mapid);
        break;
    }
    default:
        syscall_exit(-1);
    }
}

/* Checks user-provided virtual address. If it is
   invalid, terminates the current process. */
static void
check_vaddr(const void *vaddr)
{
    if (!vaddr || !is_user_vaddr(vaddr) ||
        !pagedir_get_page(thread_get_pagedir(), vaddr))
        syscall_exit(-1);
}

struct lock *syscall_get_filesys_lock(void)
{
    return &filesys_lock;
}

/* Handles halt() system call. */
static void syscall_halt(void)
{
    shutdown_power_off();
}

/* Handles exit() system call. */
void syscall_exit(int status)
{
    struct process *pcb = thread_get_pcb();
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);
    pcb->exit_status = status;
    printf("%s: exit(%d)\n", thread_name(), status);
    thread_exit();
}

/* Handles exec() system call. */
static pid_t syscall_exec(const char *cmd_line)
{
    pid_t pid;
    struct process *child;
    int i;
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    check_vaddr(cmd_line);
    for (i = 0; *(cmd_line + i); i++)
        check_vaddr(cmd_line + i + 1);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);

    pid = process_execute(cmd_line);
    child = process_get_child(pid);
    
    if (!child || !child->is_loaded)
        return PID_ERROR;
    return pid;
}

/* Handles wait() system call. */
static int syscall_wait(pid_t pid)
{
    
    return process_wait(pid);
}

/* Handles create() system call. */
static bool syscall_create(const char *file, unsigned initial_size)
{
    bool success;
    int i;

    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    check_vaddr(file);
    for (i = 0; *(file + i); i++)
        check_vaddr(file + i + 1);
    success = filesys_create(file, (off_t)initial_size);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);
    return success;
}

/* Handles remove() system call. */
static bool syscall_remove(const char *file)
{
    bool success;
    int i;
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);

    check_vaddr(file);
    for (i = 0; *(file + i); i++)
        check_vaddr(file + i + 1);

    success = filesys_remove(file);
     if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);

    return success;
}

/* Handles open() system call. */
static int syscall_open(const char *file)
{
    struct file_descriptor_entry *fde;
    struct file *new_file;
    int i;
    
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);

    check_vaddr(file);
    for (i = 0; *(file + i); i++)
        check_vaddr(file + i + 1);

    fde = palloc_get_page(0);
    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);
        return -1;
    }
    
    
    new_file = filesys_open(file);
    if (!new_file)
    {
        palloc_free_page(fde);
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);

        return -1;
    }

    fde->fd = thread_get_next_fd();
    fde->file = new_file;
    list_push_back(thread_get_fdt(), &fde->fdtelem);
    if(lock_held_by_current_thread(&filesys_lock))
      lock_release(&filesys_lock);
    
    return fde->fd;
}

/* Handles filesize() system call. */
static int syscall_filesize(int fd)
{
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    struct file_descriptor_entry *fde = process_get_fde(fd);
    int filesize;

    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);
        return -1;
    }

    
    filesize = file_length(fde->file);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);
    return filesize;
}

/* Handles read() system call. */
static int syscall_read(int fd, void *buffer, unsigned size)
{

    struct file_descriptor_entry *fde;
    int bytes_read, i;
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    for (i = 0; i < size; i++){
        if(buffer+i==NULL||!is_user_vaddr(buffer+i)){
            syscall_exit(-1);
        }
        if(find_page(buffer+i)==NULL){
            syscall_exit(-1);
        }else{
            if(!find_page(buffer+i)->writable){
                syscall_exit(-1);
            }
        }

        
    }
     
    if (fd == 0)
    {
        unsigned i;

        for (i = 0; i < size; i++)
            *(uint8_t *)(buffer + i) = input_getc();

        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);  
        return size;
    }

    fde = process_get_fde(fd);
    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);  
        return -1;
    }

   
    bytes_read = (int)file_read(fde->file, buffer, (off_t)size);
    if(lock_held_by_current_thread(&filesys_lock))
      lock_release(&filesys_lock);    
    return bytes_read;
}

/* Handles write() system call. */
static int syscall_write(int fd, const void *buffer, unsigned size)
{
    struct file_descriptor_entry *fde;
    int bytes_written, i;

    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);

    for (i = 0; i < size; i++)
        check_vaddr(buffer + i);
   
    if (fd == 1)
    {

        putbuf((const char *)buffer, (size_t)size);
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);
        return size;
    }

    fde = process_get_fde(fd);
    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);
        return -1;
    }
    
    bytes_written = (int)file_write(fde->file, buffer, (off_t)size);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);

    return bytes_written;
}

/* Handles seek() system call. */
static void syscall_seek(int fd, unsigned position)
{
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    struct file_descriptor_entry *fde = process_get_fde(fd);

    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);
        return;
    }
    
    file_seek(fde->file, (off_t)position);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);
}

/* Handles tell() system call. */
static unsigned syscall_tell(int fd)
{
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    struct file_descriptor_entry *fde = process_get_fde(fd);
    unsigned pos;

    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
            lock_release(&filesys_lock);
        return -1;
    }

    pos = (unsigned)file_tell(fde->file);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);

    return pos;
}

/* Handles close() system call. */
void syscall_close(int fd)
{
    if(!lock_held_by_current_thread(&filesys_lock))
        lock_acquire(&filesys_lock);
    struct file_descriptor_entry *fde = process_get_fde(fd);

    if (!fde){
        if(lock_held_by_current_thread(&filesys_lock))
           lock_release(&filesys_lock);
        return;
    }
    
    file_close(fde->file);
    list_remove(&fde->fdtelem);
    palloc_free_page(fde);
    if(lock_held_by_current_thread(&filesys_lock))
        lock_release(&filesys_lock);
}

static int syscall_mmap(int fd, void *addr){
    if(fd == 0 || fd == 1 || fd > 127 )
        return -1;
    if((int)addr % PGSIZE !=0)
        return -1;
    // upage is muliple of PGSIZE
    if(!addr || !is_user_vaddr(addr))
        return -1;
    lock_acquire(&filesys_lock);

    struct file_descriptor_entry *fde = process_get_fde(fd);
    if(fde==NULL){
        lock_release(&filesys_lock);
        return -1;
    }
    int page_num;
    int read_bytes = file_length(fde->file);
    int ofs = 0;
    if(read_bytes==0){
        lock_release(&filesys_lock);
        return -1;
    }
    if(read_bytes % PGSIZE == 0){
        page_num = read_bytes / PGSIZE;
    }
    else{
        page_num = read_bytes / PGSIZE + 1;
    }

    for(int i = 0; i < page_num; i++){
        if(find_page((uint8_t*)addr + i * PGSIZE)!=NULL){
            lock_release(&filesys_lock);
            return -1;
        }
    } 
    struct spte* start_page;
    for(int i=0; i < page_num; i++){
        int page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        int page_zero_bytes = PGSIZE - page_read_bytes;

        struct spte* page = malloc(sizeof(struct spte));
        if(page==NULL){
            // munmap
            lock_release(&filesys_lock);
            return -1;
        }
        if(i==0){
            start_page = page;
        }
        page->thread_id = thread_tid();
        page->related_file = file_reopen(fde->file);
        page->offset = ofs;
        page->read_bytes = page_read_bytes;
        page->zero_bytes = page_zero_bytes;
        page->writable = true;
        page->page_number = addr;
        list_push_back(&thread_current()->spt, &page->spt_elem);

        read_bytes -= page_read_bytes;
        addr += PGSIZE;
        ofs += PGSIZE;
    }
    int mapid = add_mmap_file(start_page);
    if(mapid==-1){
        // munmap
        lock_release(&filesys_lock);
        return -1;
    }
    lock_release(&filesys_lock);
    return mapid;
} 

void syscall_munmap(mapid_t mapid){
    struct list_elem* e;
    struct thread* cur = thread_current();
    for(e = list_begin(&cur->mmap_file_list); e != list_end(&cur->mmap_file_list); e = e){
        struct mmap_file* mmap_file = list_entry(e, struct mmap_file, mmap_elem);
        if(mmap_file->map_id == mapid){
            struct spte* cur_stpe = mmap_file->spte;
            struct spte* next_stpe;
            for(int i=0; i < mmap_file->page_num; i++){
                file_seek(cur_stpe->related_file, cur_stpe->offset);
                if(&cur_stpe->spt_elem != list_tail(&cur->spt)){
                    next_stpe = list_entry(list_next(&cur_stpe->spt_elem), struct spte, spt_elem);
                }

                if(pagedir_is_dirty(cur->pagedir, cur_stpe->page_number)){
                    file_write(cur_stpe->related_file, cur_stpe->frame_number, cur_stpe->read_bytes);
                // dirty        
                }
                list_remove(&cur_stpe->spt_elem);
                pagedir_clear_page(cur->pagedir, cur_stpe->page_number);
                // printf("offset: %d i: %d frame: %p\n ",cur_stpe->offset, i, cur_stpe->frame_number);
                deallocate_frame(cur_stpe->frame_number);
                free(cur_stpe);
                cur_stpe = next_stpe;
                // cur_stpe = list_remove(&cur_stpe->spt_elem);
            }
            e = list_remove(&mmap_file->mmap_elem);
            // free(mmap_file);
        }
    }
}