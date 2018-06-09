#include <stdio.h>
#include <stdlib.h>
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

#include "vm/page.h"
#include "vm/frame.h"

extern struct list lru_list;
extern struct lock lru_list_lock;

static unsigned vm_hash_func (const struct hash_elem *e, void *aux UNUSED){
// hash_entry()로 element에 대한 vm_entry 구조체 검색
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
// hash_int()를 이용해서 vm_entry의 멤버 vaddr에 대한 해시값을 구하고 반환
	return hash_int((int)vme->vaddr);
}

static bool vm_less_func (const struct hash_elem *a, const struct hash_elem *b){
// hash_entry()로 각각의 element에 대한 vm_entry 구조체를 얻은 후 vaddr 비교(b가 크다면 true, a가 크다면 false)
	struct vm_entry *vmeA = hash_entry(a, struct vm_entry, elem);
	struct vm_entry *vmeB = hash_entry(b, struct vm_entry, elem);

	if (vmeA->vaddr < vmeB->vaddr)
			return true;
	return false;
}

void vm_init (struct hash *vm){
// hash_init()으로 해시테이블 초기화
	hash_init(vm, vm_hash_func, vm_less_func, NULL);
}

bool insert_vme (struct hash *vm, struct vm_entry *vme){
// vm_entry를 해시 테이블에 삽입
	if(hash_insert(vm, &vme->elem) == NULL)
			return true;
	else
			return false;
//		return hash_insert(vm, &vme->elem) == NULL;
}

bool delete_vme (struct hash *vm, struct vm_entry *vme){
// vm_entry를 해시 테이블에서 제거
	hash_delete(vm, &vme->elem);

}

struct vm_entry *find_vme (void *vaddr){
// pg_round_down()으로 vaddr의 페이지 번호를 얻음
// hash_find() 함수를 사용해서 hash_elem 구조체 얻음
// 만약 존재하지 않는다면 NULL 리턴
// hash_entry()로 해당 hash_elem의 vm_entry 구조체 리턴
	struct vm_entry vme;
	vme.vaddr = pg_round_down(vaddr);
	struct hash_elem *e = hash_find(&thread_current()->vm, &vme.elem);
	if(!e)
			return NULL;
	return hash_entry(e, struct vm_entry, elem);
}

void vm_destroy_func(struct hash_elem *e, void *aux){
// hash element를 가져온다
	struct vm_entry *vme = hash_entry(e, struct vm_entry, elem);
// load가 되어 있는 page의 vm_entry인 경우
	if (vme->is_loaded){
// page의 할당 해제 및 page mapping 해제
		palloc_free_page(pagedir_get_page(thread_current()->pagedir, vme->vaddr));
		pagedir_clear_page(thread_current()->pagedir, vme->vaddr);
	}

// vm_entry 객체 할당 해제
	free(vme);
}

void vm_destroy (struct hash *vm){
// hash_destroy()로 해시테이블의 버킷리스트와 vm_entry들을 제거
	hash_destroy(vm, vm_destroy_func);
}

bool load_file (void *kaddr, struct vm_entry *vme){
	/*Using file_read_at()*/
		/* file_read_at으로물리페이지에read_bytes만큼데이터를씀*/
		/* file_read_at여부반환*/
		/* zero_bytes만큼남는부분을‘0’으로패딩*/
		/*정상적으로file을메모리에loading 하면true 리턴*/
	if (vme->read_bytes > 0){
		lock_acquire(&file_lock);
		if((int)vme->read_bytes != file_read_at(vme->file, kaddr, vme->read_bytes, vme->offset)){
			lock_release(&file_lock);
			return false;
		}
		lock_release(&file_lock);
		memset(kaddr + vme->read_bytes, 0, vme->zero_bytes);
	} else{
// 읽을 데이터가 없을 때
		memset(kaddr, 0, PGSIZE);
	}
		return true;
}

//페이지 할당
struct page* alloc_page(enum palloc_flags flags){
	
	void *kaddr = palloc_get_page(flags);	//페이지 할당

	while(kaddr == NULL){
/*
메모리가 부족하기 때문에 swap out을 해준다 
*/
		lock_acquire(&lru_list_lock);
//		printf("//////memory lack\n");
		kaddr = try_to_free_pages(flags);
//		printf("///////%p\n", kaddr);
		lock_release(&lru_list_lock);
	}
	
	//if(kaddr == NULL)
	//	kaddr = palloc_get_page(flags);

	struct page *page = (struct page*)malloc(sizeof(struct page));	//페이지 할당
	
	if(page == NULL){
		return NULL;
	}
//초기화
	page->kaddr = kaddr;
	page->thread = thread_current();
	page->vme = NULL;
	add_page_to_lru_list(page);	
	
	return page;
}
//물리 주소 kaddr에 해당하는 page해제
void free_page(void *kaddr){
	struct list_elem *elem;

	for(elem = list_begin(&lru_list);
		  elem != list_end(&lru_list);
			){
		struct list_elem *next_elem = list_next(elem);
		struct page *page = list_entry(elem,struct page,lru);
		if(page->kaddr == kaddr){
			__free_page(page);
			break;
		}
		elem = next_elem;
	}
}
//LRU list 내 page 해제
void __free_page(struct page* page){
	lock_acquire(&lru_list_lock);
	del_page_from_lru_list(page);
	lock_release(&lru_list_lock);
	palloc_free_page(page->kaddr);
	free(page);
}
