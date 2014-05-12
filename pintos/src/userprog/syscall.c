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
#include "user/syscall.h"
#include "devices/input.h"
#include <string.h>



#define USER_VADDR_BOTTOM ((void *) 0x0804000) //Taken from project specs

struct lock filesys_lock;

struct file_cabinet{
    struct file *file;
    int fd;
    struct list_elem elem;
};

//New helper functions declared
static void syscall_handler (struct intr_frame *);
void close_file(int fd);
void check_valid_ptr(const void * vaddr);
void check_valid_buffer(void *buffer, unsigned size);
int user_to_kernel_ptr(const void *vaddr);
void syscall_init(void);
void get_arg(struct intr_frame *f UNUSED, int *arg, int n);
struct file *process_get_file(int fd);
int process_add_file(struct file *f);


int process_add_file(struct file *f)
{
    struct file_cabinet *tempPf = malloc(sizeof(struct file_cabinet));
    tempPf -> file = f;
    tempPf -> fd = thread_current() -> fd;
    thread_current() -> fd++;
    list_push_back(&thread_current() -> file_list, &tempPf -> elem);
    return tempPf -> fd;
}

struct file *process_get_file(int fd)
{
    struct thread *tempThread = thread_current();
    struct list_elem *tempElem;

    for(tempElem = list_begin(&tempThread -> file_list); tempElem != list_end(&tempThread -> file_list); tempElem = list_next(tempElem))
    {
        struct file_cabinet *pf = list_entry(tempElem, struct file_cabinet, elem);
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
    if(!is_user_vaddr(vaddr) || vaddr < (void *) 0x08048000)
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

pid_t exec(const char* cmd_line)
{
    tid_t tmp = process_execute(cmd_line);
    char *saveptr;
    char *temp1 = strtok_r((char *)cmd_line, " ", &saveptr);
    int exist = open(temp1);
    if(exist == -1)
    {
        return -1;
    }
    close(exist);
    if(tmp == TID_ERROR)
    {
        return tmp;
    }
//    add_cp(tmp);
    return tmp;
}

void
exit(int status)
{
   struct thread* currentThread = thread_current();
   printf("%s: exit(%d)\n", currentThread -> name, status);
   thread_exit();
}

void halt(void)
{
  shutdown_power_off();
}

int wait(pid_t pid)
{
    return process_wait(pid);
}

int read(int fd, void *buffer, unsigned buffsize)
{
    if(fd == STDIN_FILENO)
    {
        uint8_t *tempBuff = (uint8_t *)buffer;
        unsigned i;
        for(i = 0; i < buffsize; i++)
        {
            tempBuff[i] = input_getc();
        }
        return buffsize;
    }
    lock_acquire(&filesys_lock);
    struct file *tempFile = process_get_file(fd);
    if(!tempFile)
    {
        lock_release(&filesys_lock);
        return -1;
    }
    int size = file_read(tempFile, buffer, buffsize);
    lock_release(&filesys_lock);

    return size;
}

int write(int fd, const void *buffer, unsigned size)
{
    lock_acquire(&filesys_lock);
    if(fd == STDOUT_FILENO)
    {
        int buf_size = 256; //Magic number
        int i = 0;
        for(i = size/buf_size; i > 0; --i)
        {
            putbuf(buffer, buf_size);
            buffer+=buf_size;
        }
        putbuf(buffer,size%buf_size);
        lock_release(&filesys_lock);
        return size;
    }
    /*
    if(fd == STDOUT_FILENO)
    {
        putbuf(buffer, size);
        return size;
    }
    lock_acquire(&fiilesys_lock);
    */
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

void close(int fd)
{
    lock_acquire(&filesys_lock);
    close_file(fd);
    lock_release(&filesys_lock);
}

void close_file(int fd)
{
    struct thread *tempThread = thread_current();
    struct list_elem *nextElem;
    struct list_elem *tempElem = list_begin(&tempThread -> file_list);
    while( !list_empty (&tempThread -> file_list) && tempElem != list_end(&tempThread -> file_list))
    {
        nextElem = list_next(tempElem);
        struct file_cabinet *pf = list_entry(tempElem, struct file_cabinet, elem);
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
    if(!file)
    {
        return false;
    }
    bool success = filesys_remove(file);
    lock_release(&filesys_lock);
    return success;
}

int filesize(int fd)
{
    lock_acquire(&filesys_lock);
    struct file *tempFile = process_get_file(fd);
    if(!tempFile)
    {
        lock_release(&filesys_lock);
        return -1;
    }
    int size = file_length(tempFile);
    lock_release(&filesys_lock);
    return size;
}

void seek(int fd, unsigned position)
{
    lock_acquire(&filesys_lock);
    struct file *tempFile = process_get_file(fd);
    if(!tempFile)
    {
        return;
    }
    file_seek(tempFile, position);
    lock_release(&filesys_lock);
    
}

unsigned tell(int fd)
{
    struct file *tempFile = process_get_file(fd);
    if(!tempFile)
    {
        return 0;
    }
    return file_tell(tempFile);
}

int open (const char *file)
{
    lock_acquire(&filesys_lock);
    struct file *tempFile = filesys_open(file);
    if(!tempFile)
    {
        lock_release(&filesys_lock);
        return -1;
    }

    int fd = process_add_file(tempFile);
    lock_release(&filesys_lock);
    return fd;
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
      get_arg(f,&args[0],1);
      args[0] = user_to_kernel_ptr((const void *) args[0]);
      f-> eax = exec((const char*)args[0]);
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
      get_arg(f,&args[0],1);
      args[0] = user_to_kernel_ptr((const void*) args[0]);
      f->eax = open((const char *) args[0]);    
      break;
    case SYS_FILESIZE:
      get_arg(f,&args[0],1);
      f->eax = filesize(args[0]);
      break;
    case SYS_READ:
      get_arg(f,&args[0],3);
      check_valid_buffer((void *) args[1],(unsigned)args[2]);
      args[1] = user_to_kernel_ptr((const void *) args[1]);
      f->eax = read(args[0], (void *) args[1],(unsigned) args[2]);
      break;
    case SYS_WRITE:
      get_arg(f, &args[0], 3);
      check_valid_buffer((void *) args[1], (unsigned) args[2]);
      args[1] = user_to_kernel_ptr((const void *) args[1]);
      f->eax = write(args[0], (const void *) args[1], (unsigned) args[2]);
      break;
    case SYS_SEEK:
      get_arg(f,&args[0],2);
      seek(args[0],(unsigned)args[1]);
      break;
    case SYS_TELL:
      get_arg(f,&args[0],1);
      f->eax = tell(args[0]);
      break;
    case SYS_CLOSE:
      get_arg(f,&args[0],1);
      close(args[0]);
      break;
    default:
      break;
  }
}

