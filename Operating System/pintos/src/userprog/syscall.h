#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"

void syscall_init (void);
void exit(int);
struct lock file_lock;
void munmap(int);
#endif /* userprog/syscall.h */
