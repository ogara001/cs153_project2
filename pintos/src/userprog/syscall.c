#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"



#define USER_VADDR_BOTTOM ((void *) 0x0804000) //Taken from project specs

struct lock filesys_lock;

struct process_file{
    struct file *file;
    int fd;
    struct list_elem elem;
};



static void syscall_handler (struct intr_frame *);
void close_file(int fd);
void exit(int status);
void check_valid_ptr(const void * vaddr);
void check_valid_buffer(void *buffer, unsigned size);
int user_to_kernel_ptr(const void *vaddr);
int write(int fd, const void *buffer, unsigned size);
void syscall_init(void);
void get_arg(struct intr_frame *f UNUSED, int *arg, int n);
struct file *process_get_file(int fd);
int wait(tid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);


struct file *process_get_file(int fd)
{
    struct thread *tempThread = thread_current();
    struct list_elem *tempElem;

    for(tempElem = list_begin(&tempThread -> file_list); tempElem != list_end(&tempThread -> file_list); tempElem = list_next(tempElem))
    {
        struct process_file *pf = list_entry(tempElem, struct process_file, elem);
        if(fd == pf -> fd)
        {
            return pf -> file;
        }
    }
    return NULL;
}

void
syscall_init(void)
{
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void check_valid_ptr(const void *vaddr)
{
    if(!is_user_vaddr(vaddr) || vaddr < USER_VADDR_BOTTOM)
    {
        exit(-1);
    }
}

void check_valid_buffer(void *buffer, unsigned size)
{
    unsigned i;
    char *local_buffer = (char *) buffer;
    for(i = 0; i < size; i++)
    {
        check_valid_ptr((const void *) local_buffer);
        local_buffer++;
    }
}

int user_to_kernel_ptr(const void *vaddr)
{
    check_valid_ptr(vaddr);
    void *ptr = pagedir_get_page(thread_current() -> pagedir, vaddr);
    if(!ptr)
    {
        exit(-1);
    }
    return (int) ptr;
}

void
exit(int status)
{
   struct thread* currentThread = thread_current();
   printf("%s: exit(%d)\n", currentThread -> name, status);
   thread_exit();
}

static void halt(void)
{
  shutdown_power_off();
}

int wait(tid_t pid)
{
    return process_wait(pid);
}

int write(int fd, const void *buffer, unsigned size)
{
    if(fd == STDOUT_FILENO)
    {
        putbuf(buffer, size);
        return size;
    }
    lock_acquire(&filesys_lock);
    struct file *f = process_get_file(fd);
    if(!f)
    {
        lock_release(&filesys_lock);
        return -1;
    }
    int bytes = file_write(f, buffer, size);
    lock_release(&filesys_lock);
    return bytes;
}

void close_file(int fd)
{
    struct thread *tempThread = thread_current();
    struct list_elem *nextElem;
    struct list_elem *tempElem = list_begin(&tempThread -> file_list);
    while( !list_empty (&tempThread -> file_list) && tempElem != list_end(&tempThread -> file_list))
    {
        nextElem = list_next(tempElem);
        struct process_file *pf = list_entry(tempElem, struct process_file, elem);
        if(fd == pf -> fd || fd == -1)
        {
            file_close(pf -> file);
            list_remove(&pf-> elem);
            free(pf);
            if(fd != -1)
            {
                return;
            }
        }
        tempElem = nextElem;
    }
}

void get_arg(struct intr_frame *f UNUSED, int *arg, int n)
{
    int i;
    int *randomPtr;
    for(i = 0; i < n; i++)
    {
        randomPtr = (int *) f-> esp + i + 1;
        check_valid_ptr((const void *) randomPtr);
        arg[i] = *randomPtr;
    }
}

bool create(const char *file, unsigned initial_size)
{
    lock_acquire(&filesys_lock);
    bool success = filesys_create(file, initial_size);
    lock_release(&filesys_lock);
    return success;
}

bool remove(const char *file)
{
    lock_acquire(&filesys_lock);
    bool success = filesys_remove(file);
    lock_release(&filesys_lock);
    return success;
}



static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int args[128];
  /*printf ("system call!\n");*/
  check_valid_ptr((const void *)f->esp);
  int sysnum = *(int*)f->esp;
  switch(sysnum)
  {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      get_arg(f, &args[0], 1);
      exit(args[0]);
      break;
    case SYS_EXEC:
      break;
    case SYS_WAIT:
      get_arg(f, &args[0], 1);
      f->eax = wait(args[0]);
      break;
    case SYS_CREATE:
      get_arg(f, &args[0],2);
      args[0] = user_to_kernel_ptr((const void *) args[0]);
      f->eax = create((const char *) args[0], (unsigned) args[1]);
      break;
    case SYS_REMOVE:
      get_arg(f, &args[0],1);
      args[0] = user_to_kernel_ptr((const void *) args[0]);
      f->eax = remove((const char *)args[0]);
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
      break;
    case SYS_WRITE:
      get_arg(f, &args[0], 3);
      check_valid_buffer((void *) args[1], (unsigned) args[2]);
      args[1] = user_to_kernel_ptr((const void *) args[1]);
      f->eax = write(args[0], (const void *) args[1], (unsigned) args[2]);
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
}

