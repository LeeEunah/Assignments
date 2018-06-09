#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
// malloc 사용을 위해
#include "threads/malloc.h"
#include "vm/page.h"

#define MAX_FILE 128

//lock 구조체 선언
struct lock file_lock;

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

int process_wait(tid_t child_tid UNUSED);
struct thread *get_child_process(int pid);
void remove_child_process(struct thread *cp);
static void argument_stack(char **parse,int count,void **esp);
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
//  true_fn: 파일 이름,  
//    copy: fn_copy를 복사한 것.
//    e.g) file_name = echo x 
//         true_fn = echo
//         copy = echo x 

	char *true_fn, *save_ptr, *copy;
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
// copy 할당	
  copy = palloc_get_page(0);

  if (fn_copy == NULL)
    return TID_ERROR;

  strlcpy (fn_copy, file_name, PGSIZE);
// file_name을 copy에 복사
	strlcpy (copy ,file_name, PGSIZE);
// 프로그램 이름을 가져옴
	true_fn = strtok_r(copy," ", &save_ptr);
	
 /* Create a new thread to execute FILE_NAME. */
// 파싱한 파일이름을 첫번째 인자에 넣어줌
  tid = thread_create (true_fn, PRI_DEFAULT, start_process, fn_copy);
 	
// 할당 해제	
	palloc_free_page(copy);
	if (tid == TID_ERROR){
    palloc_free_page (fn_copy); 
	}
  return tid;
}

//파일을 디스크립터에 추가 해주는 함수
int process_add_file(struct file *f){
	int ret;
	struct thread *t = thread_current();

//파일이 없을 때 에러
	if(f == NULL)
			return -1;


//파일 객체를 파일 디스크립터 테이블에 추가
	t->fdt[t->next_fd] = f;
//파일 디스크립터의 최대값 1 증가
	ret = t->next_fd++;
	return ret;
}


struct file *process_get_file(int fd){
	struct thread *t = thread_current();
// 디스크립터의 범위가 넘을 시에는 에러
	if(fd <= 1 || t->next_fd > MAX_FILE)
			return NULL;

// 디스크립터에 해당하는 파일 객체를 반환
	return t->fdt[fd];
}

void process_close_file(int fd){
	struct file *f = process_get_file(fd);

// file_close()를 호출해서 파일을 닫아준다
	file_close(f);
// 파일 디스크립터 테이블 해당 엔트리를 null로 초기화
	thread_current()->fdt[fd] = NULL;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
 
//parse: 파싱된 인자들의 배열
//true_fn: 프로그램 이름
//token, save_ptr: strtok_r()을 사용하기 위해
//count: 인자의 개수

	char **parse;
	char *true_fn,*token, *save_ptr;
	int i, count;

// vm 초기화
	vm_init(&thread_current()->vm);

// mmap 초기화
	list_init(&thread_current()->mmap_list);

	count = 0;
// 문자열 전체를 할당하고 파싱하고 인자의 개수도 세어준다.
//	parse = palloc_get_page(0);
	for(token = strtok_r(file_name," " , &save_ptr);
			token != NULL;
			token = strtok_r(NULL, " " , &save_ptr),count++){
		if(count != 0)
			parse = (char**)realloc(parse,sizeof(char*) * (count+1));
		else
			parse = (char**)malloc(sizeof(char*)*(count+1));

		parse[count] = (char*)malloc(sizeof(char) * strlen(token));
		strlcpy(parse[count], token, sizeof(char*) * (strlen(token)+1));
	}

//프로그램 이름을 true_fn에 넣어준다
	true_fn = parse[0];

// vm_init()함수를 이용해서 현재 쓰레드의 해시테이블 초기화
//	vm_init(&thread_current()->vm);

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

// true_fn을 load한다.
  success = load (true_fn, &if_.eip, &if_.esp);

//메모리 탑재 완료시 부모 프로세스 다시 진행
	struct thread *t = thread_current();
  /* If load failed, quit. */
  palloc_free_page (file_name);

//메모리 탑재 실패 시 프로세스 디스크립터에 메모리 탑재 실패
  if (!success){
		t->loaded = false;
		sema_up(&t->load_sema);
    thread_exit ();
	}
	t->loaded = true;
	sema_up(&t->load_sema);
	
// argument_stack()호출
	argument_stack(parse, count, &if_.esp);
//디버깅을 위한 것
//    hex_dump(if_.esp, if_.esp, PHYS_BASE - if_.esp, true);
//메모리 해제
	for(i=0; i<count; i++){
		free(parse[i]);
	}
	free(parse);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
	int i;
	
//프로세스가 종료될 때, 열린 모든 파일을 닫아준다
//파일 디스크립터 최소값인 2부터 MAX_FILE까지 닫는다
	for(i=2; i < cur->next_fd; i++){
		process_close_file(i);
	}


//실행 중인 파일 닫기
		file_close(cur->executing_file);

//파일 디스크립터 테이블 메모리 해제
	palloc_free_page((void*)cur->fdt);

// 매핑된 파일을 전부 지운다
	munmap(-1);

// vm_entry들을 제거하는 함수 추가
	vm_destroy(&cur->vm);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

//lock획득
	lock_acquire(&file_lock);

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
//lock해제
			lock_release(&file_lock);
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

//현재 실행할 파일로 초기화
	t->executing_file = file;
//파일에 대한 write를 거부
	file_deny_write(file);
//lock해제
	lock_release(&file_lock);
	
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

// 물리 페이지를 할당하고 맵핑하는 부분 삭제(done)

// vm_entry 생성(malloc 사용)
			struct vm_entry *vme = (struct vm_entry*)malloc(sizeof(struct vm_entry));

// vm_entry 멤버들 설정
			vme->type = VM_BIN;
			vme->vaddr = upage;
			vme->writable = writable;
			vme->is_loaded = false;
			vme->file = file;
			vme->offset = ofs;
			vme->read_bytes = page_read_bytes;
			vme->zero_bytes = page_zero_bytes;

// insert_vme()함수를 사용해서 생성한 vm_entry를 해시테이블에 추가
			insert_vme(&thread_current()->vm, vme);
		
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
			ofs += page_read_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  struct page *kpage;
  bool success = false;

  //kpage = alloc_page (PAL_USER | PAL_ZERO);
	//void *kaddr = kpage->kaddr;
  //if (kpage != NULL) 
  //  {
  //    success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kaddr, true);
  //    if (success)
  //     *esp = PHYS_BASE;
  //   else{
  //    free_page(kpage);
	//		}
  //  }

// vm_entry 생성
	struct vm_entry *vme = (struct vm_entry*)malloc(sizeof(struct vm_entry));
	if (vme == NULL)
			return false;
// vm_entry 멤버들 설정
	//vme = malloc(sizeof(struct vm_entry));
	vme->type = VM_ANON;
	vme->vaddr = ((uint8_t*)PHYS_BASE) - PGSIZE;
	vme->writable = true;
	vme->is_loaded = true;
	vme->pinned = false;

	struct page* page = alloc_page(PAL_USER | PAL_ZERO);
	page->vme = vme;
	void *kaddr = page->kaddr;
// insert_vme() 함수로 해시테이블에 추가
	insert_vme(&thread_current()->vm, vme);

	if(!install_page(vme->vaddr, kaddr, vme->writable)){
		free_page(page);
		return success;
	}
	
	success = true;

	if(success)
		*esp = PHYS_BASE;

	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  bool success = (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
	return success;
}

//parse = ehco asdfsadfdasfa
//count = 2
//esp = address 

// 파싱한 것을 stack에 저장하는 함수
static void argument_stack(char **parse,int count,void **esp){
	int i,j;
	uint32_t argv[count];	

//프로그램 이름 및 인자(문자열) 저장
// 파싱한 것을 메모리에 집어 넣는다
	for(i = count-1; i>=0; i--){
		for(j = strlen(parse[i]); j>=0; j--){
			*esp -= 1;
			** (char**)esp = parse[i][j];
		}
		argv[i] = (uint32_t)(*esp);
	}	
	
//word align
// Cache가 읽기 쉽게 32byte를 4byte로 끊어서 정렬해준다.
// 그렇게 함으로서 속도가 더 빨라짐
	*esp = (uint32_t) (*esp) & 0xfffffffc;
	*esp -= 4;
	memset(*esp, NULL , sizeof(int));
//프로그램 이름 및 인자를 가리키는 주소(esp) 저장
	for(i = count-1; i>=0; i--){
		*esp -= 4;
		*(uint32_t *)(*esp) = argv[i];
	}
	
//argv(문자열을 가리키는 주소들의 배열을 가리킴)
	*esp -= 4;
	*(uint32_t *)(*esp) = (uint32_t)(*esp)+4;
//argc(문자열의 개수 저장
	*esp -= 4;
	*(int *)(*esp) = (int) count;
//return address
//fake address(0)저장
	*esp -= 4;
	memset(*esp, NULL, sizeof(int));

}

//자식 프로세서 디스크립터를 검색하여 주소 리턴
struct thread *get_child_process(int pid){

	struct thread *cur = thread_current();
	struct list_elem *elem;

//리스트를 이용하여 자식 리스트에 있는 프로세스 디스크립터를 검색
	for(elem = cur->child_list.head.next;
			elem != list_end(&cur->child_list);
			elem = list_next(elem) ){
//	해당 pid의 프로세스 디스크립터가 존재하면 pid의 프로세스 디스크립터 반환
		struct thread *t = list_entry(elem, struct thread, child_elem);
		if(pid == t->tid) return t;
	}
//리스트에 존재하지 않으면 NULL 리턴
	return NULL;
}

//프로세스 디스크립터를 자식 리스트에 제거 후 메모리 해제
void remove_child_process(struct thread *cp){
	if(cp != NULL){
		list_remove(&cp->child_elem);
		palloc_free_page((void *)cp);
	}
}

//자식 프로세스가 종료될 때까지 부모 프로세스 대기
int process_wait(tid_t child_tid UNUSED){
//자식 프로세스의 프로세스 디스크립터 검색
	struct thread *child = get_child_process(child_tid);
//에외 처리 발생시 -1 리턴
	if(child == NULL) return -1;
//자식프로세스가 종료될 때까지 부모 프로세스 대기
	sema_down(&child->exit_sema);
//자식 프로세스 디스크립터 삭제 전 exit status 저장 
	int exit_status = child->exit_status;
//자식 프로세스 디스크립터 삭제
	remove_child_process(child);
//자식 프로세스의 exit status 리턴
	return exit_status;
}

// 페이지 폴트 발생 시 물리페이지를 할당
bool handle_mm_fault(struct vm_entry *vme){
	/* palloc_get_page()를이용해서물리메모리할당*/
	if(vme->is_loaded) {
			return false;
	}

	struct page *page = alloc_page(PAL_USER);
	page->vme = vme;
	void *kaddr = page->kaddr;
	
		/* switch문으로vm_entry의타입별처리(VM_BIN외의나머지타입은mmf와swapping에서다룸*/
	switch(vme->type){
		case VM_BIN:
			if(!load_file(kaddr, vme)){
				free_page(page);
				return false;
			}
			break;
	
		case VM_FILE:
			if(!load_file(kaddr, vme)){
				free_page(page);
				return false;
			}
			break;
	
		case VM_ANON:
			swap_in(vme->swap_slot, kaddr);
			break;
	}

		/* VM_BIN일경우load_file()함수를이용해서물리메모리에로드*/
		/* install_page를이용해서물리페이지와가상페이지맵핑*/
	if(install_page(vme->vaddr, kaddr, vme->writable) == false){
			free_page(kaddr);
			return false;
	}
		/* 로드성공여부반환*/
	vme->is_loaded = true;
	return true;
}

