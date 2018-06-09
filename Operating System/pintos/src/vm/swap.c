#include <stdio.h>
#include "threads/synch.h"
#include "vm/swap.h"
#include "devices/block.h"	
#include "threads/vaddr.h"	
#include "lib/kernel/bitmap.h"
#define SECTORS_PER_PAGE PGSIZE/BLOCK_SECTOR_SIZE

struct bitmap *swap_bitmap;
struct block *swap_block;
struct lock swap_lock;

/*swap 영역 초기화*/
void swap_init(){
//	printf("/////swap_init()\n");
//  size_t swap_size;
	swap_block = block_get_role(BLOCK_SWAP);

	if(!swap_block){
		return ; 
	}
 // swap_size = (size_t)block_size(swap_block);

//  printf("(%s) swap_size: %u, SECTORS_PER_PAGE: %d, bit_cnt: %u\n", __func__, swap_size, SECTORS_PER_PAGE, (size_t)swap_size/SECTORS_PER_PAGE);

	//swap_bitmap = bitmap_create(block_size(swap_block)/SECTORS_PER_PAGE);
	swap_bitmap = bitmap_create(1024);
	
	if(swap_bitmap == NULL)
		return;

	bitmap_set_all(swap_bitmap,0);
	lock_init(&swap_lock);
}
/*used_index의 swap slot에 저장된 데이터를 논리주소 kaddr로 복사*/
void swap_in(size_t used_index, void* kaddr){
//	printf("////swap_in\n");
	size_t i;

	if(!swap_block || !swap_bitmap)
			return;
	
	lock_acquire(&swap_lock);
	if(bitmap_test(swap_bitmap, used_index) == 0)
		printf("bitma_test error\n");	
	else{
//	bitmap_flip(swap_bitmap, used_index);
		for(i=0;i<SECTORS_PER_PAGE; i++)
			block_read(swap_block,
							used_index*SECTORS_PER_PAGE + i,
							(uint8_t*)kaddr + i * BLOCK_SECTOR_SIZE);
		bitmap_flip(swap_bitmap,used_index);
	}
	lock_release(&swap_lock);
}
/*kaddr주소가 가르키는 페이지를 스왑파티션에 기록
  페이지를 기록한 swap slot번호를 리턴*/ 
size_t swap_out(void* kaddr){
//	printf("//////swap_out\n");
	size_t i;
	//int index = bitmap_scan_and_flip(swap_bitmap,0,1,0);
  size_t index;

	if(!swap_block || !swap_bitmap)
			printf("error\n");

	lock_acquire(&swap_lock);
  index = bitmap_scan_and_flip(swap_bitmap,0,1,0);

	if(index == BITMAP_ERROR)
			printf("///////swap_out error\n");
	else{
		for(i=0;i<SECTORS_PER_PAGE;i++) {
  //  printf("[JATA DBG] (%s) index: %d, sector_idx: %d\n", __func__, index, index*SECTORS_PER_PAGE+i);
			block_write(swap_block,
								index*SECTORS_PER_PAGE+i,
								(uint8_t*)kaddr + i *BLOCK_SECTOR_SIZE);
  	}
	}
	
	lock_release(&swap_lock);
	return index;
}


