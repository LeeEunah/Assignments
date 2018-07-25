#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <stdio.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/buffer_cache.h"
#include "threads/thread.h"
#include "filesys/directory.h"

#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCK_ENTRIES 123
#define INDIRECT_BLOCK_ENTRIES (BLOCK_SECTOR_SIZE / sizeof (block_sector_t))

int I_BLOCK_ENTRY_NB;

enum direct_t{
  NORMAL_DIRECT,		// inode에 디스크 블록 번호를 저장
  INDIRECT,					// 1개의 인덱스 블록을 통해 디스크 블록 번호에 접근
  DOUBLE_INDIRECT,	// 2개의 인덱스 블록을 통해 디스크 블록 번호에 접근
  OUT_LIMIT					// 잘못된 파일 오프셋 값일 경우
};

struct sector_location{
  enum direct_t directness;	// 디스크 블록 접근 방법
  int index1;								// 첫 번째 index block에서 접근할 entry의 offset
  int index2; 							// 두 번째 index block에서 접근할 entry의 offset
};

struct inode_disk {
		off_t length;
		unsigned magic;
// 인덱스 방식 블록 번호
		block_sector_t direct_map_table[DIRECT_BLOCK_ENTRIES];
    block_sector_t indirect_block_sec;
    block_sector_t double_indirect_block_sec;
    uint32_t is_dir;			// 디렉토리와 파일을 구별하기 위한 변수
  };

// 인덱스 블록을 표현하는 자료구조
struct inode_indirect_block{
    block_sector_t map_table [INDIRECT_BLOCK_ENTRIES];
};

static inline size_t
bytes_to_sectors (off_t size){
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

static inline off_t map_table_offset (int index){
// byte 단위로 변환한 오프셋 값 return
		off_t base = 0;
		return base + index * sizeof(block_sector_t);
}


struct inode {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock extend_lock;						// 세마포어 락
  };

static bool get_disk_inode(const struct inode* inode, struct inode_disk *inode_disk);
static void locate_byte (off_t pos, struct sector_location *sec_loc);
static bool register_sector (struct inode_disk *inode_disk, block_sector_t new_sector, struct sector_location sec_loc);
static block_sector_t byte_to_sector (const struct inode_disk *inode_disk, off_t pos);
bool inode_update_file_length(struct inode_disk *, off_t, off_t);
static void free_inode_sectors (struct inode_disk *inode_disk);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos) {
// 반환할 디스크 블록 번호
  block_sector_t result_sec;
  if (pos < inode_disk->length){
    struct inode_indirect_block *ind_block;
    struct sector_location sec_loc;
// 인덱스 블록 offset 계산
    locate_byte (pos, &sec_loc);

    switch (sec_loc.directness){
// Direct 방식일 경우
      case NORMAL_DIRECT:
// on-disk inode의 direct_map_table에서 디스크 블록 번호를 얻음
        result_sec = inode_disk->direct_map_table[sec_loc.index1];
        break;

// Indirect 방식일 경우
      case INDIRECT:
        ind_block = (struct inode_indirect_block *) malloc (BLOCK_SECTOR_SIZE);
        if (ind_block){
// buffer cache에서 인덱스 블록을 읽어 옴
          bc_read(inode_disk->indirect_block_sec, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
// 인덱스 블록에서 디스크 블록 번호 확인
					result_sec = ind_block->map_table[sec_loc.index1];
        } else		// NULL
          result_sec = 0;
        free (ind_block);
        break;

// Double indirect 방식일 경우
      case DOUBLE_INDIRECT:
        ind_block = (struct inode_indirect_block *) malloc (BLOCK_SECTOR_SIZE);
        if (ind_block){
// 1차 인덱스 블록을 버퍼 캐시에서 읽음
          bc_read(inode_disk->double_indirect_block_sec, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
// 2차 인덱스 블록을 버퍼 캐시에서 읽음
					bc_read(ind_block->map_table[sec_loc.index1], ind_block, 0, BLOCK_SECTOR_SIZE, 0);
// 2차 인덱스 블록에서 디스크 블록 번호 확인
					result_sec = ind_block->map_table[sec_loc.index2];
        }
        else		// NULL
          result_sec = 0;
        free (ind_block);
        break;
// 그 외 예외처리
      default:
        result_sec = 0;
    }
  }
  else
    result_sec = 0;

  return result_sec;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);

  I_BLOCK_ENTRY_NB = BLOCK_SECTOR_SIZE / sizeof(block_sector_t);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, uint32_t is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL){
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
// inode 생성 시, struct inode_disk에 추가한 파일, 디렉토리 구분을 위한 필드를 is_dir인자 값으로 설정
			disk_inode->is_dir = is_dir;

      if(length > 0)
// length만큼의 디스크 블록을 inode_update_file_length()를 호출하여 할당
        if(!inode_update_file_length(disk_inode, 0, length-1))
          return false;
      
// on-disk inode를 버퍼 캐시에 기록
      bc_write(sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
// 할당받은 변수 해제
      free(disk_inode);
// success 변수 업데이트
      success = true;
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

// open_inodes 리스트에 inode가 존재하는지 검사
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) {
          inode_reopen (inode);
          return inode; 
        }
    }

// inode 자료구조 메모리 할당
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

// inode 자료구조 초기화	
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
// inode의 extend_lock 변수 초기화
  lock_init(&inode->extend_lock);
// block_read(fs_device, inode->sector, &inode->data);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
 inode_close (struct inode *inode) {
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0){
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) {
        struct inode_disk *disk_inode = malloc (sizeof (struct inode_disk));
        if (disk_inode == NULL)
						return;
// inode의 on-disk inode획득
				if(!get_disk_inode(inode, disk_inode) == false){
					return 0;
				}
// 디스크 블록 반환
				free_inode_sectors (disk_inode);
// on-disk inode 반환
        free_map_release (inode->sector, 1);
// disk_inode 변수 할당 해제
        free (disk_inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
//	uint8_t *bounce = NULL;
	
// inode_disk 자료형의 disk_inode 변수를 동적 할당
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(disk_inode == NULL)
    return 0;
// on-disk inode를 버퍼 캐시에서 읽어옴
  if(!get_disk_inode(inode, disk_inode)){
		return 0;
	}

  while (size > 0) {
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = disk_inode->length - offset;

      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (disk_inode, offset);
      if(sector_idx == 0){
        break;
      }

//			if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
//		  {
	     /* Read full sector directly into caller's buffer. */
					//block_read (fs_device, sector_idx, buffer + bytes_read);
//					bc_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
//			 }
//      else 
//	  	 {
				/* Read sector into bounce buffer, then partially copy
			    into caller's buffer. */
//		       if (bounce == NULL) 
//			      {
//				        bounce = malloc (BLOCK_SECTOR_SIZE);
//		            if (bounce == NULL)
//			             break;
//	           }
//				   block_read (fs_device, sector_idx, bounce);
//		       memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
																										        
      bc_read(sector_idx, buffer, bytes_read, chunk_size, sector_ofs);
 
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  free(disk_inode);
// free(bounce);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) {
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
// uint8_t *bounce = NULL;

// inode_disk 자료형의 disk_inode 변수를 동적 할당
  struct inode_disk *disk_inode = malloc(sizeof(struct inode_disk));
  if(disk_inode == NULL)
    return 0;
// on-disk inode를 버퍼 캐시에서 읽어옴
  if(!get_disk_inode(inode, disk_inode)){
		return 0;
	}

  if(inode->deny_write_cnt){
    free(disk_inode);
    return 0;
  }

// inode의 락 획득
  lock_acquire(&inode->extend_lock);
  int old_length = disk_inode->length;
  int write_end = offset +  size - 1;
  if(write_end > old_length -1 ){
// 파일 길이가 증가하였을 경우, on-disk inode 업데이트
    disk_inode->length = write_end + 1;
    if(!inode_update_file_length(disk_inode, old_length, write_end)){
      free(disk_inode);
      return 0;
    }
  }
// inode의 락 해제
  lock_release(&inode->extend_lock);

  while (size > 0) 
    {
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = disk_inode->length - offset;

      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (disk_inode, offset);
      if(sector_idx == 0)
        break;

      bc_write(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);

//      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
//        {
          /* Write full sector directly to disk. */
         //block_write (fs_device, sector_idx, buffer + bytes_written);
//					bc_write(sector_idx, (void*)buffer, bytes_written, chunk_size, sector_ofs);
//				}
//      else 
//        {
	        /* We need a bounce buffer. */
//          if (bounce == NULL) 
//            {
//              bounce = malloc (BLOCK_SECTOR_SIZE);
//              if (bounce == NULL)
//                break;
//            }

          /* If the sector contains data before or after the chunk
						 we're writing, then we need to read in the sector
	           first.  Otherwise we start with a sector of all zeros. */
//          if (sector_ofs > 0 || chunk_size < sector_left) 
//            block_read (fs_device, sector_idx, bounce);
//          else
//            memset (bounce, 0, BLOCK_SECTOR_SIZE);
//          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
//          block_write (fs_device, sector_idx, bounce);
//        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
// 수정한 disk_inode 자료구조를 버퍼 캐시에 기록
  bc_write(inode->sector, disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
  free(disk_inode);
// free(bounce);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  off_t result;
  struct inode_disk *disk_inode = malloc (sizeof (struct inode_disk));
  if (disk_inode == NULL)
    return 0;
  get_disk_inode (inode, disk_inode);
  result = disk_inode->length;
  free (disk_inode);
  return result;
}

bool inode_update_file_length(struct inode_disk* inode_disk, off_t start_pos, off_t end_pos){
  off_t offset = start_pos;
  int size = end_pos - start_pos + 1;
	block_sector_t sector_idx;
  uint8_t *zeroes = calloc(sizeof(uint8_t), BLOCK_SECTOR_SIZE);
  if(zeroes == NULL)
    return false;

// 블록 단위로 loop을 수행하며 새로운 디스크 블록 할당
  while(size > 0){
// 디스크 블록 내 오프셋 계산
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    int left_sector = BLOCK_SECTOR_SIZE - sector_ofs;
    int chunk_size = size < left_sector ? size : left_sector;
    if (chunk_size <= 0)
      break;

// 블록 오프셋이 0보다 클 경우, 이미 할당된 블록
    if(sector_ofs > 0){
      sector_idx = byte_to_sector(inode_disk, offset);
      bc_write(sector_idx, zeroes, 0, left_sector, sector_ofs);
    } else{
// 새로운 디스크 블록을 할당
      if(free_map_allocate(1, &sector_idx)){
// inode_disk에 새로 할당 받은 디스크 블록 번호 업데이트
        struct sector_location current_loc;
        locate_byte(offset, &current_loc);
        if(register_sector(inode_disk, sector_idx, current_loc) == false){
          free_map_release(sector_idx, 1);
          free(zeroes);
          return false;
        }
      } else{
        free(zeroes);
        return false;
      }
// 새로운 디스크 블록을 0으로 초기화
      bc_write(sector_idx, zeroes, 0, BLOCK_SECTOR_SIZE, 0);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
  }
  free(zeroes);
  return true;
}

static void free_inode_sectors (struct inode_disk *inode_disk){
  int i, j;
// Double indirect 방식으로 할당된 블록 해지
  if (inode_disk->double_indirect_block_sec > 0){
		struct inode_indirect_block *ind_block_1 = malloc (BLOCK_SECTOR_SIZE);
    struct inode_indirect_block *ind_block_2 = malloc (BLOCK_SECTOR_SIZE);
    if (ind_block_1 == NULL || ind_block_2 == NULL){
      return;
    }
// 1차 인덱스 블록을 버퍼 캐시에서 읽음
    bc_read (inode_disk->double_indirect_block_sec, ind_block_1, 0, BLOCK_SECTOR_SIZE, 0);
    i = 0;
// 1차 인덱스 블록을 통해 2차 인덱스 블록을 차례로 접근
    while (ind_block_1->map_table[i] > 0){
      bc_read (ind_block_1->map_table[i], ind_block_2, 0, BLOCK_SECTOR_SIZE, 0);
// 2차 인덱스 블록을 버퍼 캐시에서 읽음
			j = 0;
// 2차 인덱스 블록에 저장된 디스크 블록 번호를 접근
      while (ind_block_2->map_table[j] > 0){
// free_map 업데이트를 통해 디스크 블록 할당 해지
        free_map_release (ind_block_2->map_table[j], 1);
        j++;
      }
// 1차 인덱스 블록 할당 해지
      free_map_release (ind_block_1->map_table[i], 1);
      i++;
    }
// 2차 인덱스 블록 할당 해지
    free_map_release (inode_disk->double_indirect_block_sec, 1);
// 동적 할당 해지
		free (ind_block_1);
    free (ind_block_2);
  }
// Indirect 방식으로 할당된 디스크 블록 해지	
  if (inode_disk->indirect_block_sec > 0){
    struct inode_indirect_block *ind_block = malloc (BLOCK_SECTOR_SIZE);
    if (ind_block == NULL){
      return;
    }
// 인덱스 블록을 버퍼 캐시에서 읽음
    bc_read (inode_disk->double_indirect_block_sec, ind_block, 0, BLOCK_SECTOR_SIZE, 0);
    i = 0;
// 인덱스 블록에 저장된 디스크 블록 번호를 접근
    while (ind_block->map_table[i] > 0){
// free_map 업데이트를 통해 디스크 블록 할당 해지
      free_map_release (ind_block->map_table[i], 1);
      i++;
    }
    free_map_release (inode_disk->indirect_block_sec, 1);
    free (ind_block);
  }

// Direct 방식으로 할당된 디스크 블록 해지
  i = 0;
  while (inode_disk->direct_map_table[i] > 0){
// free_map 업데이트를 통해 디스크 블록 할당 해지
    free_map_release (inode_disk->direct_map_table[i], 1);
    i++;
  }
}

// inode를 버퍼캐시로부터 읽어서 전달
static bool get_disk_inode(const struct inode *inode, struct inode_disk *inode_disk){
// inode->sector에 해당하는 on-disk inode를 버퍼 캐시에 읽어 inode_disk에 저장
	bc_read(inode->sector, inode_disk, 0, BLOCK_SECTOR_SIZE, 0);
// true 반환
  return true;
}

static void locate_byte (off_t pos, struct sector_location *sec_loc){
  off_t pos_sector = pos / BLOCK_SECTOR_SIZE;
// Direct 방식일 경우
  if (pos_sector < DIRECT_BLOCK_ENTRIES){
// sec_loc 자료구조의 변수 값 업데이트
    sec_loc->directness = NORMAL_DIRECT;
    sec_loc->index1 = pos_sector;
  }
// Indirect 방식일 경우
  else if (pos_sector < (off_t) (DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES)){
// sec_loc 자료구조의 변수 값 업데이트
    sec_loc->directness = INDIRECT;
    sec_loc->index1 = pos_sector - DIRECT_BLOCK_ENTRIES;
  }
// Double Indirect 방식일 경우
  else if (pos_sector < (off_t) (DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES * (INDIRECT_BLOCK_ENTRIES + 1))){
// sec_loc 자료구조의 변수 값 업데이트
    sec_loc->directness = DOUBLE_INDIRECT;
    pos_sector = pos_sector - DIRECT_BLOCK_ENTRIES - INDIRECT_BLOCK_ENTRIES;
    sec_loc->index1 = pos_sector / INDIRECT_BLOCK_ENTRIES;
    sec_loc->index2 = pos_sector % INDIRECT_BLOCK_ENTRIES;
  }
// 잘못된 파일 오프셋일 경
  else
    sec_loc->directness = OUT_LIMIT;
}

// 새로 할당 받은 디스크 블록의 번호를 inode_disk에 업데이트
static bool register_sector (struct inode_disk *inode_disk, block_sector_t new_sector, struct sector_location sec_loc){
  struct inode_indirect_block *new_block = NULL;
  block_sector_t second_level;

  switch (sec_loc.directness){
// inode_disk에 새로 할당받은 디스크 번호 업데이트
    case NORMAL_DIRECT:
      inode_disk->direct_map_table[sec_loc.index1] = new_sector;
      break;

    case INDIRECT:
      new_block = malloc (BLOCK_SECTOR_SIZE);
      if (new_block == NULL)
        return false;

// 인덱스 블록에 새로 할당 받은 블록 번호 저장
      if (inode_disk->indirect_block_sec == false){
        if (free_map_allocate (1, &(inode_disk->indirect_block_sec))){
          memset (new_block, 0, BLOCK_SECTOR_SIZE);
          new_block->map_table[sec_loc.index1] = new_sector;
// 인덱스 블록을 버퍼 캐시에 기록
          bc_write(inode_disk->indirect_block_sec, new_block, 0, BLOCK_SECTOR_SIZE, 0);
        }
        else{
          free (new_block);
          return false;
        }
      } else{	// 이미 존재하면...
        bc_write(inode_disk->indirect_block_sec, &new_sector, 0, sizeof(block_sector_t), map_table_offset(sec_loc.index1));
      }
      break;

    case DOUBLE_INDIRECT:
      new_block = malloc (BLOCK_SECTOR_SIZE);
      if (new_block == NULL)
        return false;
// 1차 인덱스 블록에 새로 할당 받은 블록 주소 저장
      if (inode_disk->double_indirect_block_sec == false){
        if (free_map_allocate (1, &(inode_disk->double_indirect_block_sec))) {
          memset (new_block, 0, BLOCK_SECTOR_SIZE);
// 인덱스 블록을 버퍼 캐시에 기록
          bc_write(inode_disk->double_indirect_block_sec, new_block, 0, BLOCK_SECTOR_SIZE, 0);
        }
        else{
          free (new_block);
          return false;
        }
      }
      bc_read(inode_disk->double_indirect_block_sec, &second_level, 0, sizeof(block_sector_t), map_table_offset(sec_loc.index1));

// 2차 인덱스 블록이 없으면
      if (second_level == 0){
// 새로 할당 받음
        if (free_map_allocate (1, &second_level)){
// 인덱스 블록을 버퍼 캐시에 기록
          bc_write(inode_disk->double_indirect_block_sec, &second_level, 0, sizeof(block_sector_t), map_table_offset(sec_loc.index1));
          memset (new_block, 0, BLOCK_SECTOR_SIZE);
          new_block->map_table[sec_loc.index2] = new_sector;
          bc_write(second_level, new_block, 0, BLOCK_SECTOR_SIZE, 0);
        }
        else{
          free (new_block);
          return false;
        }
      }
      else{
        bc_write(second_level, &new_sector, 0, sizeof(block_sector_t), map_table_offset(sec_loc.index2));
      }
      break;

    default:
      return false;
  }

  free (new_block);
  return true;
}

bool inode_is_dir(const struct inode *inode){
	bool result;
// inode_disk 자료구조를 메모리에 할당
	struct inode_disk *on_disk_inode = malloc(sizeof(struct inode_disk));
	if(on_disk_inode == NULL)
			return 0;
// in-memory inode의 on-disk inode를 읽어 inode_disk에 저장
	bc_read(inode->sector, (void *)on_disk_inode, 0, BLOCK_SECTOR_SIZE, 0);
// on-disk inode의 is-dir을 result에 저장하여 반환
	result = on_disk_inode->is_dir;
	free(on_disk_inode);

	return result;
}

bool inode_is_removed(const struct inode *inode){
	return inode->removed;
}
