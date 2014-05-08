#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"

static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void halt(void)
{
  shutdown_power_off();
}

static void exit(int status)
{
//  printf("%s: exit(%d)\n", ...);
/* thread_exit();*/
  process_exit();
}

static void check_ptr(void *ptr)
{
  if(!is_user_vaddr(*(void**)ptr))
    exit(-1);
  return;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /*printf ("system call!\n");*/
  check_ptr(f->esp);
  int sysnum = *(int*)f->esp;
  switch(sysnum)
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit(*(int*)f->esp);
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
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
      break;
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
    default:
      break;
  }
  thread_exit ();
}

