#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

/* Number of page faults processed. */
static long long page_fault_cnt;
extern frame_table_lock;
extern frame_table;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void exception_init(void)
{
    /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
    intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
    intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
    intr_register_int(5, 3, INTR_ON, kill,
                      "#BR BOUND Range Exceeded Exception");

    /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
    intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
    intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
    intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
    intr_register_int(7, 0, INTR_ON, kill,
                      "#NM Device Not Available Exception");
    intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
    intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
    intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
    intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
    intr_register_int(19, 0, INTR_ON, kill,
                      "#XF SIMD Floating-Point Exception");

    /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
    intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void exception_print_stats(void)
{
    printf("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill(struct intr_frame *f)
{
    /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */

    /* The interrupt frame's code segment value tells us where the
     exception originated. */
    switch (f->cs)
    {
    case SEL_UCSEG:
        /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
        printf("%s: dying due to interrupt %#04x (%s).\n",
               thread_name(), f->vec_no, intr_name(f->vec_no));
        intr_dump_frame(f);
        syscall_exit(-1);

    case SEL_KCSEG:
        /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
        intr_dump_frame(f);
        PANIC("Kernel bug - unexpected interrupt in kernel");

    default:
        /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
        printf("Interrupt %#04x (%s) in unknown segment %04x\n",
               f->vec_no, intr_name(f->vec_no), f->cs);
        syscall_exit(-1);
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault(struct intr_frame *f)
{
    bool not_present; /* True: not-present page, false: writing r/o page. */
    bool write;       /* True: access was write, false: access was read. */
    bool user;        /* True: access by user, false: access by kernel. */
    void *fault_addr; /* Fault address. */

    /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
    asm("movl %%cr2, %0"
        : "=r"(fault_addr));

    /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
    intr_enable();

    /* Count page faults. */
    page_fault_cnt++;

    /* Determine cause. */
    not_present = (f->error_code & PF_P) == 0;
    write = (f->error_code & PF_W) != 0;
    user = (f->error_code & PF_U) != 0;

    /* If page fault occurs in user mode, terminates the current
     process. */
    bool is_valid = false;
    // if(user){
    //   printf("user %p\n", fault_addr);
    // }
    // if(is_user_vaddr(fault_addr)&&fault_addr>0x08048000){
    //   printf("else %p\n", fault_addr);
    // }
    // struct spte* page2 = find_page_from_frame(fault_addr);
    if (is_user_vaddr(fault_addr)&&fault_addr>0x08048000 && not_present){
      if(lock_held_by_current_thread(&frame_table_lock))
        lock_release(&frame_table_lock);
      struct spte* page = find_page(fault_addr);
      struct spte* total_page = find_page_from_spts(fault_addr);
            

      if (fault_addr > (uint8_t*)PHYS_BASE - 8 * 1024 * 1024){
        // stack growth
        uint8_t * esp;
        if(user){
          esp = f->esp;
        }
        else {
          esp = thread_current()->esp;
        }
        uint8_t *ptr = esp;
        

        if((esp - 32) <= (uint8_t *)fault_addr){
          bool success = false;
          struct frame_table_entry * frame;

          frame = allocate_frame(PAL_USER | PAL_ZERO);
          frame->is_pinned = 1;

          if (frame != NULL)
          {
              if((int)esp % PGSIZE==0)
                ptr = esp - 1;
              lock_acquire(&thread_current()->spt_lock);
              success = install_page(pg_round_down(ptr), frame->frame_number, true);
              if (success){
                  struct spte* page = malloc(sizeof(struct spte));
                  frame->mapped_page = page;

                  if(page==NULL){
                  }
                  else{
                     page->thread_id = thread_tid();
                    page->offset = 0;
                    page->read_bytes = 0;
                    page->zero_bytes = 0;
                    page->writable = true;
                    page->page_number = pg_round_down(ptr);
                    page->frame_number = frame->frame_number;
                    list_push_back(&thread_current()->spt, &page->spt_elem);
                    is_valid=true;
                     lock_acquire(&frame_table_lock); 
                    list_push_back(&frame_table, &frame->frame_elem);
                    lock_release(&frame_table_lock);
                    frame->is_pinned=0;
                  }
              }
              else
                  deallocate_frame(frame->frame_number);
              lock_release(&thread_current()->spt_lock);
          }
        }
      }
      else if (page != NULL && page->related_file != NULL){
        // normal load
        struct frame_table_entry* frame = allocate_frame(PAL_USER);
        frame->is_pinned=1;
          if(frame == NULL){
            is_valid = false;
          }
          file_seek(page->related_file, page->offset);
          // if(page->read_bytes==0){
          //   printf("readbytes\n");
          //   is_valid = false;
          // }
          if(file_read(page->related_file, frame->frame_number, page->read_bytes) != (int)page->read_bytes){
            // deallocate_frame(frame->frame_number);
            is_valid = false;

          } 
          else {

            memset(frame->frame_number + page->read_bytes, 0, page->zero_bytes);
            is_valid = true;
          }

          if(is_valid){
            lock_acquire(&thread_current()->spt_lock);
            if(!install_page(page->page_number, frame->frame_number, page->writable)){
              is_valid = false;
            }
            else {
              frame->mapped_page = page;
              page->frame_number = frame->frame_number;
              lock_acquire(&frame_table_lock); 
               list_push_back(&frame_table, &frame->frame_elem);
              lock_release(&frame_table_lock);
              frame->is_pinned=0;

            }
            lock_release(&thread_current()->spt_lock);
          }
          else{
            deallocate_frame(frame->frame_number);
          }
      }
      else if (total_page != NULL && total_page->related_file == NULL){
        // swap
        struct frame_table_entry* frame = allocate_frame(PAL_USER);
        frame->is_pinned=1;
          if(frame == NULL){
            is_valid = false;
          } else{
            is_valid = true;
          }
          if(is_valid){
            lock_acquire(&thread_current()->spt_lock);
            if(!install_page(total_page->page_number, frame->frame_number, total_page->writable)){
              deallocate_frame(frame->frame_number);
              is_valid = false;
            } else {
              
              total_page->frame_number = frame->frame_number;
              frame->mapped_page = total_page;
              swap_read(total_page->page_number, total_page->frame_number);
              lock_acquire(&frame_table_lock); 
              list_push_back(&frame_table, &frame->frame_elem);
              lock_release(&frame_table_lock);

            }
            lock_release(&thread_current()->spt_lock);
          }
      }

      // if(page){
      //   if(page->related_file!=NULL){
      //     struct frame_table_entry* frame = allocate_frame(PAL_USER);
      //     if(frame == NULL){
      //       is_valid = false;
      //     }
      //     file_seek(page->related_file, page->offset);
      //     // if(page->read_bytes==0){
      //     //   printf("readbytes\n");
      //     //   is_valid = false;
      //     // }
      //     if(file_read(page->related_file, frame->frame_number, page->read_bytes) != (int)page->read_bytes){
      //       // deallocate_frame(frame->frame_number);
      //       is_valid = false;

      //     } 
      //     else {

      //       memset(frame->frame_number + page->read_bytes, 0, page->zero_bytes);
      //       is_valid = true;
      //     }

      //     if(is_valid){
      //       if(!install_page(page->page_number, frame->frame_number, page->writable)){
      //         is_valid = false;
      //       } else {
      //         if(!lock_held_by_current_thread(&frame_table_lock)){
      //           lock_acquire(&frame_table_lock);  
      //         }
      //         frame->mapped_page = page;
      //         page->frame_number = frame->frame_number;
      //         if(lock_held_by_current_thread(&frame_table_lock))
      //           lock_release(&frame_table_lock);
      //       }
      //     } else{
      //       deallocate_frame(frame->frame_number);
      //     }
      //   }
      //   else {
      //     // swap
      //     struct frame_table_entry* frame = allocate_frame(PAL_USER);
      //     if(frame == NULL){
      //       is_valid = false;
      //     } else{
      //       is_valid = true;
      //     }
      //     if(is_valid){
      //       if(!install_page(page->page_number, frame->frame_number, page->writable)){
      //         deallocate_frame(frame->frame_number);
      //         is_valid = false;
      //       } else {
      //         if(!lock_held_by_current_thread(&frame_table_lock)){
      //           lock_acquire(&frame_table_lock);  
      //         }
      //         page->frame_number = frame->frame_number;
      //         frame->mapped_page = page;
      //         if(lock_held_by_current_thread(&frame_table_lock)){
      //           lock_release(&frame_table_lock);
      //         }
      //         swap_read(page->page_number, page->frame_number);
      //       }
      //     }
      //   }

      // } 
      // else {
      //   // stack growth

      //   if(!lock_held_by_current_thread(&frame_table_lock)){
      //     lock_acquire(&frame_table_lock);  
      //   }
      //   uint8_t * esp;
      //   if(user){
      //     esp = f->esp;
      //   }
      //   else {
      //     esp = thread_current()->esp;
      //   }
      //   uint8_t *ptr = esp;
        

      //   if((esp - 32) <= (uint8_t *)fault_addr){
      //     bool success = false;
      //     struct frame_table_entry * frame;


      //     frame = allocate_frame(PAL_USER | PAL_ZERO);
      //     if (frame != NULL)
      //     {
      //         if((int)esp % PGSIZE==0)
      //           ptr = esp - 1;
      //         success = install_page(pg_round_down(ptr), frame->frame_number, true);
      //         if (success){
      //             struct spte* page = malloc(sizeof(struct spte));
      //             frame->mapped_page = page;

      //             if(page==NULL){
      //               is_valid = false;
      //             }
      //             page->thread_id = thread_tid();
      //             page->offset = 0;
      //             page->read_bytes = 0;
      //             page->zero_bytes = 0;
      //             page->writable = true;
      //             page->page_number = pg_round_down(ptr);
      //             page->frame_number = frame->frame_number;
      //             list_push_back(&thread_current()->spt, &page->spt_elem);
      //             is_valid=true;
      //         }
      //         else
      //             deallocate_frame(frame->frame_number);
      //     }
      //   }
      //   if(lock_held_by_current_thread(&frame_table_lock)){
      //       lock_release(&frame_table_lock);
      //   }
      // }
      
    }

    /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
    if(!is_valid){
      // printf("aa %p %p\n", fault_addr, page2);
      // printf("bb %p \n",find_page_from_spts(fault_addr));
      syscall_exit(-1);
      // printf("Page fault at %p: %s error %s page in %s context.\n",
      //      fault_addr,
      //      not_present ? "not present" : "rights violation",
      //      write ? "writing" : "reading",
      //      user ? "user" : "kernel");
      // kill(f);  
    }
    
}
