#include <stdio.h>
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"

struct list lru_list; 
struct lock lru_list_lock;
struct list_elem *lru_clock;

//LRU list초기화
void lru_list_init(void){
	list_init(&lru_list);
	lock_init(&lru_list_lock);
	lru_clock = NULL;
}

//LRU list의 끝에 유저페이지 삽입
void add_page_to_lru_list(struct page* page){
	lock_acquire(&lru_list_lock);
	list_push_back(&lru_list,&page->lru);
	lock_release(&lru_list_lock);
}

//LRU list에 유저 페이지 제거
void del_page_from_lru_list(struct page* page){
//	lock_acquire(&lru_list_lock);
	list_remove(&page->lru);
//	lock_release(&lru_list_lock);	
}

//LRU list리스트의 next리스트를 반환
static struct list_elem* get_next_lru_clock(){
	if(lru_clock==NULL)
		return NULL;
	if(list_size(&lru_list)==1)	
		return NULL;
	struct list_elem *next_elem = list_next(lru_clock);
	
	if(next_elem == list_end(&lru_list)){
//		return NULL;
		next_elem = list_begin(&lru_list);
	}
	if(next_elem == list_end(&lru_list))
			next_elem = NULL;
	return next_elem;
}
//물리 페이지가 부족할 때 clock알고리즘을 이용하여 여유메모리 확보
void *try_to_free_pages(enum palloc_flags flags){
//	struct list_elem *first = lru_clock;
//	printf("/////try\n");

	if(list_empty(&lru_list) == true){
//		printf("/////here??\n");
		lru_clock == NULL;
		return NULL;
	}

	if(lru_clock == NULL)
		lru_clock = list_begin(&lru_list);
	
	while(lru_clock){
		struct list_elem *next = get_next_lru_clock();
		
		struct page *page = list_entry(lru_clock, struct page, lru);

//		lru_clock = next;
		if(!page->vme->pinned){
			struct thread *t = page->thread;
//			printf(".....%d\n", t->pagedir);
			if(pagedir_is_accessed(t->pagedir,page->vme->vaddr)){
				pagedir_set_accessed(t->pagedir,page->vme->vaddr,false);
			}
			else{

				if(pagedir_is_dirty(t->pagedir,page->vme->vaddr ||
					 page->vme->type == VM_ANON)){
//				if(page->vme->type == VM_FILE){
//					lock_acquire(&file_lock);
					file_write_at(page->vme->vaddr,
											page->kaddr,
											page->vme->read_bytes,
											page->vme->offset);
					lock_release(&file_lock);
//				}else if(page->vme->type != VM_ERROR){
//					page->vme->type = VM_ANON;
//					page->vme->swap_slot = swap_out(page->kaddr);
				}
//				printf("/////ANON\n");
				else{
				page->vme->type = VM_ANON;
				page->vme->swap_slot = swap_out(page->kaddr);
//				printf("/////%d\n", page->vme->swap_slot);
			
				}
			page->vme->is_loaded=false;
			del_page_from_lru_list(page);
			pagedir_clear_page(t->pagedir,page->vme->vaddr);
			palloc_free_page(page->kaddr);
			free(page);
			lru_clock = next;

			return palloc_get_page(flags);
		}
	}
		lru_clock = next;
	}
//	return NULL;
}
