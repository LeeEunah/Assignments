#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer_cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;
struct lock filesys_lock;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

//	bc_init();
  inode_init ();
	bc_init();
  free_map_init ();

	lock_init(&filesys_lock);

  if (format) 
    do_format ();

  free_map_open ();

// struct thread에서 추가한 필드를 root 디렉터리로 설정
	thread_current()->cur_dir = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;

	char *cp_name = malloc(strlen(name) + 1);
	if (cp_name == NULL)
			return false;

// name의 파일 경로를 cp_name에 복사
	strlcpy(cp_name, name, strlen(name)+1);

// cp_name의 경로 분석
	char file_name[NAME_MAX +1];
	struct dir *dir = parse_path(cp_name, file_name);
//  struct dir *dir = dir_open_root ();
	if(dir == NULL)
			return false;

	free(cp_name);

	if(inode_is_removed(dir_get_inode(dir)))
			return NULL;

	lock_acquire(&filesys_lock);

	bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
									// inode의 is_dir값 설정
                  && inode_create (inode_sector, initial_size, 0)
									// 추가되는 디렉토리 엔트리의 이름을 file_name으로 수정
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
	lock_release(&filesys_lock);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
//  struct dir *dir = dir_open_root ();
// 생성하고자 하는 파일경로인 name을 cp_name에 복사
	char *cp_name = malloc(strlen(name)+1);
	if (cp_name == NULL)
			return false;
	strlcpy(cp_name, name, strlen(name)+1);

	char file_name[NAME_MAX+1];
// 경로 분석
	struct dir *dir = parse_path(cp_name, file_name);
	if (dir == NULL)
			return false;

	free(cp_name);

	if(inode_is_removed(dir_get_inode(dir)))
			return NULL;

// 디렉토리 엔트리에서 file_name을 검색하도록 수정
	lock_acquire(&filesys_lock);
	struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

	struct file *file = file_open(inode);
	lock_release(&filesys_lock);
	return file;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
//  struct dir *dir = dir_open_root ();
//  bool success = dir != NULL && dir_remove (dir, name);
//  dir_close (dir); 
	char *cp_name = malloc(strlen(name)+1);
	if(cp_name == NULL)
			return false;
	strlcpy(cp_name, name, strlen(name)+1);

	char file_name[NAME_MAX + 1];
	struct dir *dir = parse_path(cp_name, file_name);
	if(dir == NULL)
			return false;

	free(cp_name);

	if(inode_is_removed(dir_get_inode(dir)))
			return NULL;

	struct inode *inode = NULL;
	if(dir != NULL)
		dir_lookup(dir, file_name, &inode);

	bool success = false;

	if(inode != NULL && inode_is_dir(inode)){
// 디렉토리가 삭제 가능한지 판단
		bool is_deletable = true;

		struct inode *cp_inode = inode_reopen(inode);
		struct dir *child_dir = dir_open(cp_inode);
		char dir_name[NAME_MAX + 1];

		while(dir_readdir(child_dir, dir_name)){
			if(strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0)
					continue;
			else{
				is_deletable = false;
				break;
			}
		}
		dir_close(child_dir);
// file_name이 디렉토리일 경우 디렉토리내에 파일이 존재하지 않으면 삭제
		success = is_deletable && dir_remove(dir, file_name);
	} else{
// file_name이 파일일 경우 삭제
			success = (dir != NULL && dir_remove(dir, file_name));
		}
	inode_close(inode);
	dir_close(dir);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");

// 파일 시스템 포맷 시, 루트 디렉토리 엔트리에 '.', '..' 파일 추가
	struct dir *root = dir_open_root();
	if(!dir_add(root, ".", ROOT_DIR_SECTOR))
			return;
	if(!dir_add(root, "..", ROOT_DIR_SECTOR))
			return;
	dir_close(root);
	free_map_close ();
  printf ("done.\n");
}

struct dir* parse_path(char *path_name, char *file_name){
	struct dir *dir;
	
	if (path_name == NULL || file_name == NULL)
			return NULL;

	if (strlen(path_name) == 0)
			return NULL;

// PATH_NAME의 절대/상대경로에 따른 디렉토리 정보 저장
	if (path_name[0] == '/')	// root에서 시작
			dir = dir_open_root();
	else	// 현재 디렉토리에서 시작
			dir = dir_reopen(thread_current()->cur_dir);

	char *token, *nextToken, *savePtr;
	token = strtok_r(path_name, "/", &savePtr);
	nextToken = strtok_r(NULL, "/", &savePtr);

	while(token != NULL && nextToken != NULL){
		struct inode *inode;
/* dir에서 token이름의 파일을 검색하여 inode의 정보를 저장*/
/* inode가 파일일 경우 NULL 반환*/
		bool success = dir_lookup(dir, token, &inode);
		if(!success || !inode_is_dir(inode)){
			return NULL;
		}

		struct dir *nextDir = dir_open(inode);
/* dir의 디렉터리 정보를 메모리에서 해지*/
		dir_close(dir);
/* inode의 디렉터리 정보를 dir에 저장*/
		dir = nextDir;
/* token에 검색할 경로 이름 저장*/
		token = nextToken;
		nextToken = strtok_r(NULL, "/", &savePtr);
	}

	if(token == NULL)
			token = ".";
// token의 파일 이름을 file_name에 저장
	size_t size = strlcpy(file_name, token, NAME_MAX+1);

	if(size > NAME_MAX+1)
			return NULL;

// dir 정보 반환
	return dir;
}

// name 경로에 파일 생성하도록 변경하는 함수
bool filesys_create_dir(const char *name){
	block_sector_t inode_sector = 0;
/* name 경로 분석*/
	char *cp_name = malloc(strlen(name) + 1);
	if(cp_name == NULL)
			return false;
	strlcpy(cp_name, name, strlen(name) +1);

	char file_name[NAME_MAX+1];
	struct dir *dir = parse_path(cp_name, file_name);
	if(dir == NULL)
			return false;

	free(cp_name);

	if(inode_is_removed(dir_get_inode(dir)))
			return NULL;
	
	struct dir *newdir=NULL;
	bool success = (dir != NULL
					// bitmap에서 inode sector번호 할당
					&& free_map_allocate(1, &inode_sector)
					// 할당받은 sector에 file_name의 디렉토리 생성
					&& dir_create(inode_sector, 16)
					// 디렉토리 엔트리에 file_name의 엔트리 추가
					&& dir_add(dir, file_name, inode_sector)
					&& (newdir = dir_open(inode_open(inode_sector)))
					// 디렉토리 엔트리에 '.', '..' 파일의 엔트리 추가
					&& dir_add(newdir, ".", inode_sector)
					&& dir_add(newdir, "..", inode_get_inumber(dir_get_inode(dir))));

	if(!success && inode_sector != 0)
					free_map_release(inode_sector, 1);
	dir_close(dir);
	dir_close(newdir);

	return success;

}
