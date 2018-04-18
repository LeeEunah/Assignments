#include "userprog/syscall.h"
#include <stdio.h>
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
#include "threads/malloc.h"

void check_address(void*);
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


static void syscall_handler (struct intr_frame *);

//lock 구조체 선언
struct lock file_lock;

void
syscall_init (void) 
{
//	printf("init\n");
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

	check_address(f->esp);

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
				f->eax = exec((const char*)arg[0]);
				break;
		
		case SYS_WAIT:
				get_argument(f->esp,arg,1);
				f->eax = wait ((int)arg[0]);
				break;
// 주소값을 인자로 받는 경우 사용자가 접근하는 주소가 올바른지 여부를 확인
		case SYS_CREATE:
				get_argument(f->esp, arg, 2);
// 반환값을 가져온다.
				f->eax = create((const char*)arg[0], (unsigned)arg[1]);
				break;

		case SYS_REMOVE:
				get_argument(f->esp, arg, 1);
				f->eax = remove((const char*)arg[0]);
				break;

		case SYS_OPEN:
				get_argument(f->esp, arg, 1);
				f->eax = open((const char*)arg[0]);
				break;

		case SYS_FILESIZE:
				get_argument(f->esp, arg, 1);
				f->eax = filesize((int)arg[0]);
				break;

		case SYS_READ:
				get_argument(f->esp, arg, 3);
				f->eax = read((int)arg[0], (void*)arg[1], (unsigned)arg[2]);
				break;
	
		case SYS_WRITE:
				get_argument(f->esp, arg, 3);
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
	}
}

// 시스템 콜 인자로 받은 주소값이 유저 영역에 있는 것인지 확인
void check_address(void *addr){
 if(!((void*)0x8048000 < addr && addr < (void*)0xc0000000))
		 exit(-1);
}

void get_argument(void *esp, int *arg, int count){
	int i;
// esp의 꼭대기에는 system number가 저장되어 있으므로 +4를 한다.
	esp += 4;
// stack이 커널 영역을 침범하지 않았는지 확인
	check_address(esp);
	check_address(esp + (count * 4));

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
	check_address((void*)file);
	return filesys_create(file, initial_size);
}

bool remove(const char *file){
	check_address((void*)file);
	return filesys_remove(file);
}

//자식프로세스를 생성하고 프로그램을 실행시키는 시스템 콜
tid_t exec(const char *cmd_line){
	check_address((void*)cmd_line);
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
	check_address((void*)file);
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
//파일 디스크립터를 이용하여 파일 객체 검색
	struct file *f = process_get_file(fd);

//파일이 존재하지 않을 때
	if(!f)
			return -1;

//파일 길이를 리턴
	return file_length(f);
}

int read(int fd, void *buffer, unsigned size){
	check_address(buffer);
	unsigned i;
//파일 디스크립터를 이용해서 파일 객체 검색
// open을 제외한 모든 file관련 시스템 콜은 fd를 file포인터로 바꿔줘야함
	struct file *f = process_get_file(fd);
//파일에 동시 접근이 일어날 수 있으므로 lock사용
	lock_acquire(&file_lock);

//error검사
	if(fd < 0)	return -1;
	if(size < 0) return -1;

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
	check_address(buffer);
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
	struct file *f = process_get_file(fd);
//offset을 position만큼 이동
	file_seek(f, position);
}

unsigned tell(int fd){
	struct file *f = process_get_file(fd);
//offset을 반환
	return file_tell(f);
}

void close(int fd){
// 해당 파일 디스크립터에 해당하는 파일을 닫고 엔트리 초기화
	process_close_file(fd);
}
