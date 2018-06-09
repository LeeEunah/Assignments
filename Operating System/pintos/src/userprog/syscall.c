#include "userprog/syscall.h"
#include <stdio.h>
#include <list.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
// 스레드를 종료시키는 함수
#include <threads/thread.h>
// 핀토스를 종료시키는 함수
#include <devices/shutdown.h>
// 파일과 관련된 함수
#include <filesys/filesys.h>
#include <filesys/file.h>
#include <devices/input.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/page.h"

struct vm_entry* check_address(void*, void*);
void check_valid_buffer(void*, unsigned, void*, bool);
void check_valid_string(const void*, void*);
void get_argument(void*, int*, int);

//System Call
void halt(void);
void exit(int);
bool create(const char*, unsigned);
bool remove(const char*);
tid_t exec(const char *cmd_line);
int wait(tid_t tid);
int open(const char*);
int filesize(int fd);
int read(int, void*, unsigned);
int write(int, void*, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);
int mmap(int, void*);

static void syscall_handler (struct intr_frame *);

//lock 구조체 선언
//struct lock file_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

//파일 lock 초기화
	lock_init(&file_lock);
}


// user stack에 저장된 system call number를 통해 호출할 수 있게 하는 함수
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
// swtich문에서 사용할 시스템콜 넘버
	int syscall_nr = *(int*)f->esp;
// 인자들을 담을 배열을 선언
	int arg[5];

	check_address(f->esp, f->esp);

	switch(syscall_nr){
		case SYS_HALT:
				halt();
				break;

		case SYS_EXIT:
// stack에 있는 인자들의 데이터를 저장한다
				get_argument(f->esp, arg, 1);
				exit((int)arg[0]);
				break;

		case SYS_EXEC:
				get_argument(f->esp,arg,1);
				check_valid_string((const void*)arg[0], f->esp);
				f->eax = exec((const char*)arg[0]);
				break;
		
		case SYS_WAIT:
				get_argument(f->esp,arg,1);
				f->eax = wait ((int)arg[0]);
				break;
// 주소값을 인자로 받는 경우 사용자가 접근하는 주소가 올바른지 여부를 확인
		case SYS_CREATE:
				get_argument(f->esp, arg, 2);
				check_valid_string((const void *)arg[0], f->esp);
// 반환값을 가져온다.
				f->eax = create((const char*)arg[0], (unsigned)arg[1]);
				break;

		case SYS_REMOVE:
				get_argument(f->esp, arg, 1);
				check_valid_string((const void*)arg[0], f->esp);
				f->eax = remove((const char*)arg[0]);
				break;

		case SYS_OPEN:
				get_argument(f->esp, arg, 1);
				check_valid_string((const void*)arg[0], f->esp);
				f->eax = open((const char*)arg[0]);
				break;

		case SYS_FILESIZE:
				get_argument(f->esp, arg, 1);
				f->eax = filesize((int)arg[0]);
				break;

		case SYS_READ:
				get_argument(f->esp, arg, 3);
				check_valid_buffer((void*)arg[1], (unsigned)arg[2], f->esp, true);
				f->eax = read((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
				break;
	
		case SYS_WRITE:
				get_argument(f->esp, arg, 3);
				check_valid_buffer((void*)arg[1], (unsigned)arg[2], f->esp, false);
				f->eax = write((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
				break;

		case SYS_SEEK:
				get_argument(f->esp, arg, 2);
				seek((int)arg[0], (unsigned)arg[1]);
				break;

		case SYS_TELL:
				get_argument(f->esp, arg, 1);
				f->eax = tell((int)arg[0]);
				break;

		case SYS_CLOSE:
				get_argument(f->esp, arg, 1);
				close((int)arg[0]);
				break;

		case SYS_MMAP:
				get_argument(f->esp, arg, 2);
				f->eax = mmap((int)arg[0], (void*)arg[1]);
				break;

		case SYS_MUNMAP:
				get_argument(f->esp, arg, 1);
				munmap((int)arg[0]);
				break;

		default:
				thread_exit();
	}
}

// 시스템 콜 인자로 받은 주소값이 유저 영역에 있는 것인지 확인
struct vm_entry* check_address(void *addr, void* esp){
 if(!((void*)0x8048000 < addr && addr < (void*)0xc0000000))
		 exit(-1);
	
 bool load;
 struct vm_entry* vme = find_vme((void*)addr);
// addr이 vm_entry에 존재하면 vm_entry를 반환하도록 코드 작성
	if(vme){
		vme->pinned = true;
		handle_mm_fault(vme);
		load = vme->is_loaded;
	}

	if(load == false){
		exit(-1);
	}
	
	return vme;
}

void check_valid_buffer (void *buffer, unsigned size, void *esp, bool to_write){
	char* cur_buffer = (char *) buffer;
	unsigned i;
// buffer ~ buffer + size까지 주소에 대해 적용
	for(i=0; i<size; i++){
// 주소의 유저영역 여부를 검사한 vm_entry 구조체를 얻는다
		struct vm_entry* vme = check_address((const void*)cur_buffer, esp);
// vm_entry 존재여부와 writable이 true인지 검사
		if (vme && to_write){
			if(!vme->writable){
				exit(-1);
			}
		}
		cur_buffer++;
	}
}

// 인자의 문자열의 주소값이 유효한 가상주소인지 검사
void check_valid_string (const void *str, void *esp){
	void* cur_str = (void*)str;
	
	check_address(cur_str, esp);

	while(*(char*) str != 0){
			str = (char *) str + 1;
			check_address(str, esp);
	}

}

void get_argument(void *esp, int *arg, int count){
	int i;
// esp의 꼭대기에는 system number가 저장되어 있으므로 +4를 한다.
	esp += 4;
// stack이 커널 영역을 침범하지 않았는지 확인
	check_address(esp, esp);
	check_address(esp + (count * 4), esp);

// 인자 개수만큼 esp에 저장해준다.
	for(i=0; i<count; i++){
		arg[i] = *(int*)(esp+(i*4));	
	}
	
}

void halt(void){
	shutdown_power_off();
}

void exit(int status){
	struct thread *t = thread_current();
//프로세스 디스크립터에 exit status 저장
	t->exit_status = status;
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size){
	lock_acquire(&file_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&file_lock);
	return success;
}

bool remove(const char *file){
	lock_acquire(&file_lock);
	bool success = filesys_remove(file);
	lock_release(&file_lock);
	return success;
}

//자식프로세스를 생성하고 프로그램을 실행시키는 시스템 콜
tid_t exec(const char *cmd_line){
//명령어의 해당하는 프로그램을 수행하는 프로세스 생성
	tid_t tid = process_execute(cmd_line);
//생성된 자식 프로세스의 프로세스 디스크립터를 검색
	struct thread *child = get_child_process(tid);
//자식 프로세스의 프로그램이 탑재될때까지 대기

	if(child == NULL)
		return -1;

	sema_down(&child->load_sema);
//프로그램 탑재 실패시 -1 리턴
	if(!child->loaded) {
		return -1;
	}


	else return tid;
//프로그램 탑재 성공 시 자식 프로세스 pid 리턴
}

int wait(tid_t tid){
//자식 프로세스가 종료 될 때까지 대기
	return process_wait(tid);
}

int open(const char *file){
	lock_acquire(&file_lock);
// 파일을 open	
	struct file *f = filesys_open(file);
	int fd;

//파일이 존재하지 않을 때
	if(!f){
		lock_release(&file_lock);		
		return -1;
	}

//해당 파일 객체에 파일 디스크립터 부여
	fd = process_add_file(f);
	lock_release(&file_lock);
//파일 디스크립터를 리턴
	return fd;
}

int filesize(int fd){
	lock_acquire(&file_lock);
//파일 디스크립터를 이용하여 파일 객체 검색
	struct file *f = process_get_file(fd);

//파일이 존재하지 않을 때
	if(!f){
		lock_release(&file_lock);
		return -1;
	}

//파일 길이를 리턴
	int size = file_length(f);
	lock_release(&file_lock);
	return size;
}

int read(int fd, void *buffer, unsigned size){
	unsigned i;
//파일 디스크립터를 이용해서 파일 객체 검색
// open을 제외한 모든 file관련 시스템 콜은 fd를 file포인터로 바꿔줘야함
	struct file *f = process_get_file(fd);
//파일에 동시 접근이 일어날 수 있으므로 lock사용
	lock_acquire(&file_lock);

//error검사
	if(fd < 0)	return -1;
// if(size < 0) return -1;
// input_getc()를 이용해서 키보드 입력을 버퍼에 저장 후 그 크기를 리턴
	if(fd == 0){
		for(i=0; i<size; i++){
			*((char *)buffer+i) = input_getc();
		}
			lock_release(&file_lock);
			return i;
	}

//파일이 존재하지 않을 때
	if(f==NULL){
		lock_release(&file_lock);
		return -1;
	}

	lock_release(&file_lock);
	return file_read(f, buffer, size);

}

int write(int fd, void *buffer, unsigned size){
	struct file *f = process_get_file(fd);
//lock사용
	lock_acquire(&file_lock);

	if(f){
		lock_release(&file_lock);
		return file_write(f, buffer, size);
	} else if (fd == 1){
// putbuf()를 이용해서 버퍼에 저장된 값을 화면에 출력하고 버퍼 크기를 리턴
		putbuf(buffer, size);
		lock_release(&file_lock);
		return size;
	}
	lock_release(&file_lock);
	return -1;
}

void seek(int fd, unsigned position){
	lock_acquire(&file_lock);
	struct file *f = process_get_file(fd);
	if(!f){
		lock_release(&file_lock);
		return;
	}
//offset을 position만큼 이동
	file_seek(f, position);
	lock_release(&file_lock);
}

unsigned tell(int fd){
	lock_acquire(&file_lock);
	struct file *f = process_get_file(fd);
	if(!f){
		lock_release(&file_lock);
		return -1;
	}
//offset을 반환
	off_t ofs = file_tell(f);
	lock_release(&file_lock);
	return ofs;
}

void close(int fd){
	lock_acquire(&file_lock);
// 해당 파일 디스크립터에 해당하는 파일을 닫고 엔트리 초기화
	process_close_file(fd);
	lock_release(&file_lock);
}

// 요구 페이징에 의해 파일 데이터를 메모리로 로드하는 함수
int mmap (int fd, void* addr){
// 프로세스의 vm공간에 매핑할 파일을 가져온다
	struct file *f = process_get_file(fd);
	struct file *rf;
	struct mmap_file *mf;
	int32_t offset = 0;
	uint32_t read_bytes;
	int mapid = 0;

// 주소가 유효한지 검사
	if (!f || !addr || (uint32_t)addr <= 0x8048000 || (uint32_t)addr >= 0xc0000000 || (uint32_t) addr % PGSIZE != 0)
			return -1;

	rf = file_reopen(f);
	read_bytes = file_length(rf);

// 재오픈한 파일이 없을 때
	if (!rf || read_bytes == 0)
			return -1;

// mmap_file 구조체 멤버 설정
	mf = (struct mmap_file*)malloc(sizeof(struct mmap_file));
	list_init(&mf->vme_list);
	mf->file = rf;
	mf->mapid = mapid++;

// 파일을 다 읽을 때까지
	while(read_bytes > 0){
		uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		uint32_t page_zero_bytes = PGSIZE - page_read_bytes;

// 해당 주소에 값이 존재하면 해당 mapid에 매핑된 것을 해제
		if (find_vme(addr) != NULL){
			munmap(mf->mapid);
			return -1;
		}

		struct vm_entry *vme;
		if (!(vme = (struct vm_entry*)malloc(sizeof(struct vm_entry)))){
			munmap(mf->mapid);
			return -1;
		}

// vme 초기화
		vme->file = rf;
		vme->offset = offset;
		vme->vaddr = addr;
		vme->read_bytes = page_read_bytes;
		vme->zero_bytes = page_zero_bytes;
		vme->writable = true;
		vme->is_loaded = false;
		vme->type = VM_FILE;
		vme->pinned = false;

// vme의 mmap_elem을 mmap_file의 리스트로 삽입
		list_push_back(&mf->vme_list, &vme->mmap_elem);
		insert_vme(&thread_current()->vm, vme);

// 다음으로
		read_bytes -= page_read_bytes;
		offset += page_read_bytes;
		addr += PGSIZE;
	}

	list_push_back(&thread_current()->mmap_list, &mf->elem);

	return mf->mapid;
}

// mmap_file의 vme_list에 연결된 모든 vm_entry들을 제거
void do_munmap(struct mmap_file* mmap_file){
	struct list_elem *e, *next;
	struct file *f = mmap_file->file;

	e = list_begin(&mmap_file->vme_list);
// vme_list 탐색
	while (e != list_end(&mmap_file->vme_list)){
		next = list_next(e);

		struct vm_entry *vme = list_entry(e, struct vm_entry, mmap_elem);

		if (vme->is_loaded){
// dirty하면 디스크에 메모리 내용을 기록
			if (pagedir_is_dirty(thread_current()->pagedir, vme->vaddr)){
				lock_acquire(&file_lock);
				file_write_at(vme->file, vme->vaddr, vme->read_bytes, vme->offset);
				lock_release(&file_lock);
			}

// vme페이지 삭제
			free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));

//			palloc_free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
			pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
		}

		list_remove(&vme->mmap_elem);
		delete_vme(&thread_current()->vm, vme);

		free(vme);
		e = next;
	}

	if (f){ //열었던 파일 닫기
		lock_acquire(&file_lock);
		file_close(f);
		lock_release(&file_lock);
	}
}

// mmap_list내에서 매핑에 해당하는 mapid를 갖는 모든 vme를 해제
void munmap(int mapping){
	struct list_elem *e, *next;

	e = list_begin(&thread_current()->mmap_list);

// mmap_list 탐색
	while (e != list_end(&thread_current()->mmap_list)){
		struct mmap_file* mmap_file = list_entry (e, struct mmap_file, elem);
		next = list_next(e);

// 해당하는 mapid가 있을 때, 모두 unmap하고 싶을 때 제거
		if(mmap_file->mapid == mapping || mapping == -1){
			do_munmap(mmap_file);
			list_remove(&mmap_file->elem);
			free(mmap_file);
			if(mapping != -1)
					break;
		}
		e = next;
	}
}
