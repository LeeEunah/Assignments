#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/page.h"

struct thread *get_child_process (int);
struct file *process_get_file (int);

int process_add_file (struct file *);
void process_close_file (int);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool handle_mm_fault(struct vm_entry*);

#endif /* userprog/process.h */
