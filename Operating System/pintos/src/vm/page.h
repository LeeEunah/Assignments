#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include <list.h>
#include <threads/synch.h>

#define VM_BIN 0
#define VM_FILE 1
#define VM_ANON 2
#define VM_ERROR 3

struct vm_entry{
	uint8_t type;							// VM_BIN, VM_FILE, VM_ANON의 타입
	void *vaddr;							// vm_entry의 가상페이지 번호
	bool writable;						// True일 경우 해당 주소에 write 가능
														// False일 경우 해당 주소에 write 불가능
	bool is_loaded;						// 물리메모리의 탑재 여부를 알려주는 플래그
	bool pinned;
	struct file* file;				// 가상주소와 맵핑된 파일

	struct list_elem mmap_elem;	// mmap 리스트 element

	size_t offset;						// 읽어야 할 파일 오프셋
	size_t read_bytes;				// 가상페이지에 쓰여져 있는 데이터 크기
	size_t zero_bytes;				// 0으로 채울 남은 페이지의 바이트

	size_t swap_slot;					// 스왑 슬롯

	struct hash_elem elem;		// 해시 테이블 Element
};

struct mmap_file {
	int mapid;								// mmap() 성공 시 리턴된 mapping id
	struct file* file;				// 맴핑하는 파일의 파일 오브젝트
	struct list_elem elem;		// mmap_file들의 리스트 연결을 위한 구조체
	struct list vme_list;			// mmap_file에 해당하는 모든 vm_entry들의 리스트
};

struct page{
	void *kaddr;							 //페이지의 물리주소
	struct vm_entry *vme;			 //물리 페이지가 매핑된 가상 주소의 vm_entry 포인터
	struct thread *thread;     //해당 물리 페이지를 사용중인 스레드의 포인터
	struct list_elem lru;		   //list연결을 위한 필드
};

void vm_init (struct hash*);
struct vm_entry* find_vme (void*);
bool insert_vme (struct hash*, struct vm_entry*);
bool delete_vme (struct hash*, struct vm_entry*);
void vm_destroy (struct hash*);

bool load_file(void*, struct vm_entry*);

#endif
