#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_bUFFER_cACHE_H

#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

struct buffer_head{
	bool dirty;							// 해당 entry가 dirty인지 나타내는 flag
	bool valid;							// 해당 entry의 사용 여부를 나타내는 flag
	block_sector_t sector;	// 해당 entry의 disk sector 주소
	bool clock_bit;					// clock algorithm을 위한 clock bit
	struct lock lock;				// lock 변수
	void *data;							// buffer cache entry를 가리키기 위한 데이터 포인터
};

bool bc_read(block_sector_t, void*, off_t, int, int);
bool bc_write(block_sector_t, void*, off_t, int, int);
void bc_init(void);
struct buffer_head* bc_lookup(block_sector_t);
struct buffer_head* bc_select_victim(void);
void bc_flush_entry(struct buffer_head*);
void bc_flush_all_entries(void);

#endif
