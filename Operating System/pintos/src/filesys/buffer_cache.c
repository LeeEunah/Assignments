#include "filesys/buffer_cache.h"
#include <stdio.h>
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#define BUFFER_CACHE_ENTRY_NB 64	// buffer cache entry의 개수

// buffer head 배열
struct buffer_head buffer_head[BUFFER_CACHE_ENTRY_NB];
void* p_buffer_cache; 	// buffer cache 메모리 영역을 가리킴
static int clock_hand;	// victim entry 선정 시 clock 알고리즘을 위한 변수

// buffer cache에서 데이터를 읽어 유저 buffer에 저장
bool bc_read (block_sector_t sector_idx, void *buffer, off_t buffer_ofs, int chunk_size, int sector_ofs){
// sector_idx를 buffer_head에서 검색
  struct buffer_head *bh = bc_lookup(sector_idx);
  
  if(!bh){		// 검색 결과가 없을 때,
// 디스크 블록을 캐싱 할 buffer_entry의 buffer_head를 구함
    bh = bc_select_victim();
    if(!bh){
      return false;
    }
    lock_acquire(&bh->lock);
// 디스크 블록 데이터를 buffer cache로 read
    block_read(fs_device, sector_idx, bh->data);

    /* update buffer head */
    bh->dirty = false;
    bh->valid = true;
    bh->sector = sector_idx;
    lock_release(&bh->lock);
  }
  lock_acquire(&bh->lock);
// buffer에 디스크 블록 데이터를 복사
  memcpy(buffer + buffer_ofs, bh->data + sector_ofs, chunk_size);
// clock bit setting
  bh->clock_bit = true;
  lock_release(&bh->lock);

  return true;
}

bool bc_write (block_sector_t sector_idx, void *buffer, off_t buffer_ofs, int chunk_size, int sector_ofs){
// sector_idx를 buffer_head에서 검색
	struct buffer_head *bh = bc_lookup(sector_idx);
  
  if(!bh){	// 검색 결과가 없을 때,
// 디스크 블록을 캐싱 할 buffer entry의 buffer_head를 구함
		bh = bc_select_victim();
		if(!bh){
	  	return false;
		}
// 디스크 블록 데이터를 buffer cache로 read
	block_read(fs_device, sector_idx, bh->data);
  }
 
  lock_acquire(&bh->lock);
// buffer에 복사
  memcpy(bh->data + sector_ofs, buffer + buffer_ofs, chunk_size);

// buffer head 업데이트	
  bh->dirty = true;
  bh->valid = true;
  bh->sector = sector_idx;
// clock bit setting
	bh->clock_bit = true;
  lock_release(&bh->lock);

  return true;
}

// buffer cache 영역 할당 및 buffer_head 자료구조 초기화
void bc_init(void){
  int i;
  void* cache;  

// 메모리에 buffer cache 할당
  p_buffer_cache = malloc(BLOCK_SECTOR_SIZE*BUFFER_CACHE_ENTRY_NB);
  if(!p_buffer_cache){
    return;
  }
  else{
// p_buffer_cache가 buffer cache 영역 포인팅
    cache = p_buffer_cache;
  }

// 전역 변수 buffer_head 자료구조 초기화	
  for(i=0; i<BUFFER_CACHE_ENTRY_NB; i++){
    buffer_head[i].dirty = false;
    buffer_head[i].valid = false;
    buffer_head[i].sector = -1;
    buffer_head[i].clock_bit = 0;
    lock_init(&buffer_head[i].lock);
    buffer_head[i].data = cache;

    cache += BLOCK_SECTOR_SIZE;
  }

}

void bc_term(void){
// 모든 buffer cache entry를 디스크로 flush
  bc_flush_all_entries();
// buffer cache 영역 할당 해제
  free(p_buffer_cache);
}

struct buffer_head* bc_lookup(block_sector_t sector){
	int i;
// buffer_head를 순회
  for(i = 0; i < BUFFER_CACHE_ENTRY_NB; i++){
// 전달받은 sector값과 동일한 sector값을 갖는 buffer cache entry가 있는지 확인
    if(buffer_head[i].sector == sector){
      return &buffer_head[i];
    }
  }
// 성공: buffer_head반환, 실패: NULL
  return NULL;
}

struct buffer_head* bc_select_victim(void)
{
  int i;
  
// clock알고리즘을 사용하여 victim entry를 선택
// buffer_head를 순회하며 clock bit 변수를 검사
  while(true){
    i = clock_hand;
    if(i == BUFFER_CACHE_ENTRY_NB)
      i = 0;

    if(++clock_hand == BUFFER_CACHE_ENTRY_NB)
      clock_hand = 0;

    if(buffer_head[i].clock_bit){
      lock_acquire(&buffer_head[i].lock);
      buffer_head[i].clock_bit = 0;
      lock_release(&buffer_head[i].lock);
    }
    else{
      lock_acquire(&buffer_head[i].lock);
      buffer_head[i].clock_bit = 1;
      lock_release(&buffer_head[i].lock);
      break;
    }
  }

// 선택된 victim entry가 dirty일 경우, 디스크로 flush
  if(buffer_head[i].dirty){
    bc_flush_entry(&buffer_head[i]);
  }

// victim entry에 해당하는 buffer_head 값 update
  lock_acquire(&buffer_head[i].lock);
  buffer_head[i].dirty = false;
  buffer_head[i].valid = false;
  buffer_head[i].sector = -1;
  lock_release(&buffer_head[i].lock);

// victim entry return
  return &buffer_head[i];
}

void bc_flush_entry(struct buffer_head *p_flush_entry){
  lock_acquire(&p_flush_entry->lock);
// 인자로 전달받은 buffer cache entry의 데이터를 디스크로 flush
  block_write(fs_device, p_flush_entry->sector, p_flush_entry->data);
// buffer_head의 dirty 값 update
	p_flush_entry->dirty = false;
  lock_release(&p_flush_entry->lock);
}

void bc_flush_all_entries(void)
{
  int i;
// buffer_head를 순회
  for(i = 0; i < BUFFER_CACHE_ENTRY_NB; i++){
// dirty인 entry는 block_write()를 호출해서 디스크로 flush
		if(buffer_head[i].dirty){
      lock_acquire(&buffer_head[i].lock);
	  	block_write(fs_device, buffer_head[i].sector, buffer_head[i].data);
// 디스크로 flush한 후, dirty값 update
			buffer_head[i].dirty = false;
      lock_release(&buffer_head[i].lock);
		}
  }
}

