/*
Filesystem Lab disigned and implemented by Liang Junkai,RUC
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fuse.h>
#include <errno.h>
#include "disk.h"

#define DIRMODE S_IFDIR|0755
#define REGMODE S_IFREG|0644
#define INODE_SIZE (sizeof(struct inode))
#define SUPERBLK 0
#define IBITMAP 1
#define DBITMAP1 2
#define DBITMAP2 3
#define INODE_START 4
#define DATA_NODE_START 1028
#define FILE_NAME_SIZE 28
#define MAX_FILE_NUM 32768
#define INODE_NUM_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define MAX_DATA_NUM (BLOCK_NUM - DATA_NODE_START)
#define DIR_SIZE (sizeof(struct directory))
#define MAX_DIR_NUM (((BLOCK_SIZE / DIR_SIZE) - 1))
#define ROOT_NUM 0
#define MAX_POINTER_NUM ((BLOCK_SIZE / sizeof(int)) - 1)

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define DETAIL 0

struct vfsstate{
	unsigned long  f_bsize; 
	fsblkcnt_t     f_blocks;
	fsblkcnt_t     f_bfree; 
	fsblkcnt_t     f_bavail;
	fsfilcnt_t     f_files; 
	fsfilcnt_t     f_ffree; 
	fsfilcnt_t     f_favail;
	unsigned long  f_namemax;
}; 

struct sblock
{
	int inode_num;
	int datanode_num;
	int inode_start_block;
	int dnode_start_block;
};

struct inode
{
	mode_t mode;
	uid_t uid;
	off_t size;
	time_t time;
	time_t mtime;
	time_t ctime;
	time_t dtime;
	gid_t gid;
	nlink_t links_count;
	int blocks;
	int block[16];
};

struct directory{
	char file_name[FILE_NAME_SIZE];
	int inode_num;
};

void print_inode_content(struct inode content) {
	printf("print_inode_content :\n----------\n");
	printf("%d\n", content.blocks);
	for (int i = 0;i < content.blocks;++i)
		printf("%d ", content.block[i]);
	printf("\n");
	printf("--------------\n");
}

void print_directory(struct directory * ptr, int num) {
	int i;
	printf("print_dir:\n-------------\n");
	printf("%d\n", num);
	for (i = 0;i < num; ++i)
		printf("%s %d\n", (ptr + i) -> file_name, (ptr + i) -> inode_num);
	printf("-------\n");
}

struct inode init_inode_content(mode_t file_mode);    //初始化inode

char* get_father_path(char * path);                		//获取父目录的路径
char * get_direct_file_name(char * path);             //获取父目录的文件名

struct vfsstate get_supernode();											//读取superblock内容
char * get_inode_bitmap();														//读取inodebitmap内容
char * get_data_bitmap();															//读取datablock bitmap内容
int get_inode_bit_stat(int inode_num);                //读取inodebitmap某一位的值
int get_data_bit_stat(int data_num);                  //读取datablockbitmap某一位的值
int get_directory_num(int dir_pos);										//读取目录下的文件数
void get_directory(int dir_pos, struct directory * dir_pointer);
																											//读取目录内容
struct inode get_inode_by_num(int inode_num);					//根据块号读取inode
int get_inode_by_path(char * path);
int get_indirect_num(int data_num)										//获取间接指针的块号
void get_indirect_block(int data_pos, int * inode_pointer)  //读取indirectpointer指向的datablock

void write_supernode(struct vfsstate fs);							//写入superblock
void write_inode_bitmap(char* inode_bitmap);					//写入inodebitmap
void write_data_bitmap(char * data_bitmap);						//写入databitmap
void write_inode_bitmap_stat(int inode_num);					//改变inodebit状态
void write_data_bitmap_stat(int data_num);						//改变datablock状态
void write_inode_by_num(int inode_num, struct inode inode_content);    //根据块号写入inode
void write_empty_datablock(int data_num);							//写入空的datablock
void write_directory(int dir_pos, int dir_num, struct directory * dir_pointer);   //写目录
void write_inode_by_path(const char * path, struct inode inode_content);    //根据路径写入inode
void write_indirect_block(int data_pos, int direct_pointer_num, int * inode_num_pointer);   //写入indirect

int find_directory_by_inode_and_name(int * inode_num_pointer, int inode_num, char * file_name);   //根据inode和name找到目录
int find_directory_by_name(struct directory * dir_pointer, int dir_num, char * file_name);        //根据name找到目录
int find_directory_by_num(struct directory * dir_pointer, int dir_num, int inode_pos);						//根据块号找到目录
int find_free_inodeblock();																//找到空闲的inode块
int find_free_datablock();																	//找到空闲的data block
int find_and_delete_directory(int inode_pos, int inode_num, int * dir_inode_num_array);    //寻找并删除目录
int find_datablock_by_num(struct inode inode_content, int block_num);				//根据块号找到data block
int find_indirect_by_num(struct inode inode_content, int block_num);				//根据块号找到indirect

void rm_update_father_dir(const char * path, int inode_num);
void delete_directory(struct directory * dir_pointer, int dir_num, int delete_pos);
int insert_new_file(const char * path, int inode_num);
int insert_directory_item(int dir_pos, struct directory dir);

char * get_father_path(char * path)
{
	int len = strlen(path);
	char * f_path = (char *)malloc(sizeof(char) * len);
	int pos = len - 1;	//路径字符串最后一位
	while(path[pos] != '/')	//找到最后一个/的位置
	{
		pos--;
	}
	for(int i = 0; i < pos; ++i)
	{
		f_path[i] = path[i];
	}
	f_path[pos] = '\0';
	return f_path;
}

char * get_direct_file_name(char * path/*fpath*/)
{
	int len = strlen(path);
	char * f_name = (char*)malloc(sizeof(char) * len);
	int pos = len - 1;
	while(path[pas] != '/')
	{
		pos--;
	}
	for(int i = pos + 1; i < len; ++ i)
	{
		f_name[i - pos - 1] = path[i];
	}
	f_name[len - pos - 1] = '\0';
	return f_name;
}

struct inode init_inode_content(mode_t file_mode)
{
	struct inode inode_content;

	inode_content.mode = file_mode;
	inode_content.nlinkk = 1;
	inode_content.uid = getuid();
	inode_content.gid = getgid();
	inode_content.atime = inode_content.mtime = inode_content.ctime = 0;
	if(file_mode == (DIRMODE))   //目录模式
	{
		inode_content.block = 1;    //目录初始分配一个datablock
		inode_content.size = 0;
	}
	else //文件模式
	{
		inode_content.blocks = 0;   //文件模式初始不分配datablock
		inode_content.size = 0;
	}

	return inode_content;
}

struct vfsstate get_supernode()
{
	struct vfsstate fsinfo;
	char fsinfo_content[BLOCK_SIZE];
	disk_read(SUPERBLK, fsinfo_content);
	fsinfo = *(struct vfsstate *)fsinfo_content;
	return fsinfo;
}

char * get_inode_bitmap()												//读取inodebitmap内容
{
	char * inode_bitmap = (char *)malloc(BLOCK_SIZE * sizeof(char));
	disk_read(IBITMAP, inode_bitmap);
	return inode_bitmap;
}

char * get_data_bitmap()												//读取datablock bitmap内容
{
	char * data_bitmap = (char *)malloc(2 * BLOCK_SIZE * sizeof(char));
	disk_read(DBITMAP1, data_bitmap);
	disk_read(DBITMAP2, data_bitmap);
	return data_bitmap;
}

int get_inode_bit_stat(int inode_num)                //读取inodebitmap某一位的值
{
	char * inode_bitmap = read_inode_bitmap();

	int pos = inode_num / 8;
	int off = inode_num % 8;
	int val = (inode_bitmap[pos] >> off) & (0x1);
	free(inode_bitmap);
	return val;
}

int get_data_bit_stat(int data_num)                  //读取datablockbitmap某一位的值
{
	char * data_bitmap = read_inode_bitmap();

	int pos = data_num / 8;
	int off = data_num % 8;
	int val = (data_bitmap[pos] >> off) & (0x1);
	free(data_bitmap);
	return val;
}

int get_directory_num(int dir_pos)										//读取目录块号
{
	char content[BLOCK_SIZE];
	disk_read(DATA_NODE_START + dir_pos, content);
	int num = *(int *)(content);   //获取目录下的文件数
	return num;
}

void get_directory(int dir_pos, struct directory * dir_pointer)
																											//读取目录内容
{
	char content[BLOCK_SIZE];
	disk_read(DATA_NODE_START + dir_pos, content);
	int num = *(int *)(content);
	for(int i = 0; i < num; ++i)
	{
		//读取目录文件中的文件指针
		*(dir_pointer + i) = *(struct directory *)(content + (i + 1)* DIR_SIZE);
	}
}

struct inode get_inode_by_num(int inode_num)					//根据块号读取inode
{
	int pos = inode_num / INODE_NUM_PER_BLOCK;     //inode 所在块号
	int off = inode_num %  INODE_NUM_PER_BLOCK;	   //指定的inode在块中的偏移量

	char content[BLOCK_SIZE];
	disk_read(INODE_START + pos, content);

	struct inode * inode_content_ptr = (struct inode *)malloc(INODE_SIZE);
	inode_content_ptr = (struct inode *)(content + offset * INODE_SIZE;

	return inode_content_ptr[off];
}

int get_indirect_num(int data_pos)										//获取间接指针的块号
{
	char content[BLOCK_SIZE;
	disk_read(DATA_NODE_START + data_pos, content);
	int indirect_pointer_num = *(int *)(content);
	if(indirect_pointer_num <= 0)
		return 0;
	else
		return indirect_pointer_num;
}

void get_indirect_block(int data_pos, int * inode_pointer)  //读取indirectpointer指向的datablock
{
	char content[BLOCK_SIZE];
	disk_read(DATA_NODE_START + data_pos, content);
	int indirect_pointer_num = *(int *)(content);
	for(int i = 0; i < direct_pointer_num; ++i)
	{
		*(inode_num_pointer + i) = *(int *)(content + (i + 1) * sizeof(int));
	}
}

int get_inode_by_path(char * path)
{
	struct inode dir_inode = get_inode_by_num(ROOT_NUM);
	struct inode file_inode;
	int file_inode_num = 0;
	int st = 1, len = strlen(path);
	char file_name[FILE_NAME_SIZE];
	while(st < len)
	{
		int off = 0;
		while(path[st] != '/' && st < len)
		{
			file_name[off++] = path[st++];
		}
		file_name[off++] = 0;
		st++; 
		int flag = find_directory_by_inode_and_name(dir_inode.block, MIN(14, dir_inode.blocks), file_name);
		if(flag == -1)
		{
			for(int i = 0; i < MIN(2, dir_inode.blocks - 14); ++i)
			{
				int indirect_num = get_indirect_num(dir_inode.block[i + 14]);
				int * inode_num_pointer = (int *)malloc(sizeof(int) * dorect_num);
				get_indirect_block(dir_inode.block[i + 14], inode_num_pointer);

				flag = find_directory_by_inode_and_name(inode_num_pointer, direct_num, file_name);
				free(inode_num_pointer);
			}
		}
		if(flag != -1)
		{
			file_inode_num = flag;
		}
		else return -1;
		file_inode = get_inode_by_num(file_inode_num);
		dir_inode = file_inode;
	}
	return file_inode_num;

}

void write_supernode(struct vfsstate fs)							//写入superblock
{
	char * char_fs = (char *)(&fs);
	char content[BLOCK_SIZE];
	memcpy(content, char_fs, sizeof(struct vfsstate));
	disk_write(SUPERBLK, content);
}

void write_inode_bitmap(char* inode_bitmap)					//写入inodebitmap
{
	disk_write(IBITMAP, inode_bitmap);
}

void write_data_bitmap(char * data_bitmap)						//写入databitmap
{
	disk_write(DBITMAP1, data_bitmap);
	disk_write(DBITMAP2, data_bitmap + BLOCK_SIZE);
}

void write_inode_bitmap_stat(int inode_num)					//改变inodebit状态
{
	char * inode_bitmap = get_inode_bitmap();

	int pos = inode_num / 8;
	int off = inode_num % 8;
	struct vfsstate fs;
	fs = get_supernode();
	int flag = (inode_bitmap[pos] >> off) & (0x1);

	if(flag == 1)
	{
		fs.f_ffree += 1;
		fs.f_favail += 1;
	}
	else
	{
		fs.f_ffree -= 1;
		fs.f_favail -= 1;
	}

	write_supernode(fs);
	inode_bitmap[pos] = inode_bitmap[pos] ^ (1 << off);

	write_inode_bitmap(inode_bitmap);
	free(inode_bitmap);
}
void write_data_bitmap_stat(int data_num)						//改变datablock状态
{
	char * data_bitmap = get_data_bitmap();
	int pos = data_num / 8;
	int off = data_num % 8;
	
	struct vfsstate fs;
	fs = get_supernode();
	int flag = (data_bitmap[pos] >> off) & (0x1);

	if (flag == 1) {
		fs.f_bfree += 1;
		fs.f_bavail += 1;
	}
	else {
		fs.f_bfree -= 1;
		fs.f_bavail -= 1;
	}
	
	write_supernode(fs);
	
	data_bitmap[pos] = data_bitmap[pos] ^ (1 << off);	
	
	write_data_bitmap(data_bitmap);
	free(data_bitmap);
	return flag;
}

void write_inode_by_num(int inode_num, struct inode inode_content)   //根据块号写入inode
{
	int pos = inode_num / INODE_NUM_PER_BLOCK;
	int off = inode_num % INODE_NUM_PER_BLOCK;
	
	char content[BLOCK_SIZE];
	disk_read(INODE_START + pos, content);

	memcpy(content + INODE_SIZE * off, (char *)(&inode_content), INODE_SIZE);

	disk_write(INODE_START + pos, content);
}

void write_empty_datablock(int data_num);							//写入空的datablock
{
	char content[BLOCK_SIZE];
	int zero = 0;
	char * char_zero = (char *)(&zero);
	memcpy(content, char_zero, sizeof(int));
	disk_write(DATA_NODE_START + data_num, content);
}

void write_directory(int dir_pos, int dir_num, struct directory * dir_pointer)   //写目录
{
	char content[BLOCK_SIZE];
	char * char_num = (char *)(&dir_num);
	memcpy(content, char_num, sizeof(int));
	char * char_dir_pointer = (char *)(dir_pointer);
	memcpy(content + DIR_SIZE, char_dir_pointer, DIR_SIZE);

	disk_write(DATA_NODE_START + dir_pos, content);
}

void write_inode_by_path(const char * path, struct inode inode_content)    //根据路径写入inode
{
	struct inode dir_inode = get_inode_by_num(ROOT_NUM);
	struct inode file_inode;
	int i = 0;
	int st = 0, len = strlen(path);
	char file_name[28];
	int file_inode_num = 0;
	while(st < len)
	{
		int off = 0;
		while(st < len && path[st] != '/')
		{
			file_name[off++] = path[st++];
		}
		file_name[off++] = '\0';
		st++;
		int flag = find_directory_by_inode_and_name(dir_node.block, MIN(14, dir_inode.blocks), file_name);
		if(flag == -1)
		{
			for(i = 0; i < MIN(2, dir_inode.blocks - 14); ++i)
			{
				int indirect_num = get_indirect_num(dir_inode.block[i + 14]);
				int * inode_num_pointer = (int *)malloc(sizeof(int) * indirect_num);
				get_indirect_block(dir_inode.block[i + 14])
			}
		}

	}
}

void write_indirect_block(int data_pos, int direct_pointer_num, int * inode_num_pointer)   //写入indirect
{
	char content[BLOCK_SIZE];

	char * char_num_pointer = (char*)(& direct_pointer_num);
	memcpy(content, char_num_pointer, sizeof(int));

	char * char_inode_num_pointer = (char *)inode_num_pointer;
	memcpy(content, char_inode_num_pointer, sizeof(int));

	disk_write(DATA_NODE_START + data_pos, content);
}

int find_directory_by_inode_and_name(int * inode_num_pointer, int inode_num, char * file_name)   //根据inode和name找到目录
{
	int j;
	for(j = 0; j < inode_num; ++j)
	{
		int dir_num = get_directory_num(inode_num_pointer);
		struct directory * dir_pointer = (struct directory*)malloc(dir_num * dir_size);
		get_directory(inode_num_pointer[j], dir_pointer);
		int flag = find_by_name(dir_pointer, dir_num, file_name);
		if(flag != -1)
		{
			return flag;
		}
		return -1;
	}
}

int find_directory_by_name(struct directory * dir_pointer, int dir_num, char * file_name)        //根据name找到目录
{
	int j;
	for(j = 0; j < dir_num; j++)
	{
		if(strcmp((dir_pointer + j) -> file_name, file_name) == 0)
		{
			return (dir_pointer + j) ->inode_num;
		}
	}
	return -1;
}

int find_directory_by_num(struct directory * dir_pointer, int dir_num, int inode_pos)						//根据块号找到目录
{
	int j;
	for(j = 0; j < dir_num; j ++)
	{
		if((dir_pointer + j) -> inode_num == INODE_START)
			return j;
	}
	return -1;
}

int find_free_inodeblock()																//找到空闲的inode块
{
	int i;
	for(i = 0; i < MAX_FILE_NUM; ++i)
	{
		if(get_inode_bit_stat(i) == 0)
		{
			return i;
		}
	}
	return -1;
}

int find_free_datablock()
{
	int i = 0;
	for(i = 0; i < MAX_DATA_NUM; ++i)
	{
		if(get_data_bit_stat(i) == 0)
		{
			return i;
		}
	}
	return -1;
}	
															//找到空闲的data block
int find_and_delete_directory(int inode_pos, int inode_num, int * dir_inode_num_array)    //寻找并删除目录
{
	int flag = -1;
	for(int i = 0; i < inode_num; ++i)
	{
		int dir_num = get_directory_num(dir_inode_num_array[i]);
		struct directory * dir_pointer = (struct directory *)malloc(dir_num * dir_size);
		get_directory(dir_inode_num_array[i], dir_pointer);

		flag = find_directory_by_num(dir_pointer, dir_num, INODE_START);
		if(flag == -1)
		{
			continue;
		}
		delete_directory(dir_pointer, dir_num, flag);
		write_directory(dir_inode_num_array[i], dir_num - 1, dir_pointer);
		return flag;
	}
}

int find_datablock_by_num(struct inode inode_content, int block_num)				//根据块号找到data block
{
	if(block_num < 14)
	{
		return inode_content.block[block_num];
	}
	int data_num, block_off;
	if(block_num < 14 + MAX_DIR_NUM)
	{
		data_num = inode_content.block[14];
		block_off = block_num - 14 - MAX_DIR_NUM;
	}
	char content[BLOCK_SIZE];
	disk_read(DATA_NODE_START + data_num, content);
}

int find_indirect_by_num(struct inode inode_content, int block_num)				//根据块号找到indirect
{
	if(block_num < 14)
	{
		return 0;
	}
	else
	{
		if(block_num < 14 + MAX_DIR_NUM)
		{
			return 14;
		}
		else return 15;
	}
}

void rm_update_father_dir(const char * path, int inode_num)
{
	char * father_path = get_father_path(path);
	struct inode father_inode = get_inode_by_num(get_inode_by_path(father_path));
	
	int i;
	int flag = -1;
	flag = find_and_delete_directory(inode_num, MIN(14, father_inode.blocks), father_inode.block);

	if(flag == -1)
	{
		for(i = 0; i < MIN(2, father_inode.blocks - 14); ++i)
		{
			int direct_num = get_indirect_num(father_inode.block[i + 14]);
			int * inode_num_pointer = (int *)malloc(sizeof(int) * direct_num);
			get_indirect_block(father_inode.block[i + 14], inode_num_pointer);

			flag = find_and_delete_directory(inode_num, direct_num, inode_num_pointer);
			free(inode_num_pointer);
		}
	}
	father_inode.mtime = time();
	father_inode.ctime = time();
	write_inode_by_path(father_path, father_inode);
	free(father_path);
}

void delete_directory(struct directory * dir_pointer, int dir_num, int delete_pos)
{
	int j = delete_pos;
	while (j < dir_num - 1) 
	{
		* (dir_pointer + j) = * (dir_pointer + j + 1);
		j++;
	}
}

int insert_new_file(const char * path, int inode_num)
{
	char * father_path = get_father_path(path);
	char * file_name = get_direct_file_name(path);
	struct inode father_inode = get_inode_by_num(get_inode_by_path(father_path));\

	struct directory dir;
	strcpy(dir.file_name, file_name);
	dir.inode_num = inode_num;

	int i, j;
	int insert_flag = -1;

	for(i = 0; i < MIN(14, father_inode.blocks); ++i)
	{
		insert_flag = insert_directory_item(father_inode.block[i], dir);
		if(insert_flag != -1)
		{
			break;
		}
	}
	
}
int insert_directory_item(int dir_pos, struct directory dir);



//Format the virtual block device in the following function
int mkfs()
{
	struct sblock *s;
	s->inode_num = (BLOCK_NUM / 2);
	s->datanode_num = DATA_NODE_NUM;
	s->inode_start_block = 4;
	s->dnode_start_block = 1 + 1 + 2 + BLOCK_NUM / (2 * BLOCK_SIZE / INODE_SIZE)  - 1;
	disk_write(0, (void*)s);
	char init[BLOCK_SIZE] = 0;
	disk_write(1, (void*)init);
	disk_write(2, (void*)init);
	disk_write(3, (void*)init);
	return 0;
}




//Filesystem operations that you need to implement
int fs_getattr (const char *path, struct stat *attr)
{
	printf("Getattr is called:%s\n", path);
	return 0;
}

int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	printf("Readdir is called:%s\n", path);
	return 0;
}

int fs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("Read is called:%s\n", path);
	return 0;
}

int fs_mknod (const char *path, mode_t mode, dev_t dev)
{
	printf("Mknod is called:%s\n",path);
	return 0;
}

int fs_mkdir (const char *path, mode_t mode)
{
	printf("Mkdir is called:%s\n",path);
	return 0;
}

int fs_rmdir (const char *path)
{
	printf("Rmdir is called:%s\n",path);
	return 0;
}

int fs_unlink (const char *path)
{
	printf("Unlink is callded:%s\n",path);
	return 0;
}

int fs_rename (const char *oldpath, const char *newname)
{
	printf("Rename is called:%s\n",path);
	return 0;
}

int fs_write (const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("Write is called:%s\n",path);
	return 0;
}

int fs_truncate (const char *path, off_t size)
{
	printf("Truncate is called:%s\n",path);
	return 0;
}

int fs_utime (const char *path, struct utimbuf *buffer)
{
	printf("Utime is called:%s\n",path);
	return 0;
}

int fs_statfs (const char *path, struct statvfs *stat)
{
	printf("Statfs is called:%s\n",path);
	return 0;
}

int fs_open (const char *path, struct fuse_file_info *fi)
{
	printf("Open is called:%s\n",path);
	return 0;
}

//Functions you don't actually need to modify
int fs_release (const char *path, struct fuse_file_info *fi)
{
	printf("Release is called:%s\n",path);
	return 0;
}

int fs_opendir (const char *path, struct fuse_file_info *fi)
{
	printf("Opendir is called:%s\n",path);
	return 0;
}

int fs_releasedir (const char * path, struct fuse_file_info *fi)
{
	printf("Releasedir is called:%s\n",path);
	return 0;
}

static struct fuse_operations fs_operations = {
	.getattr    = fs_getattr,
	.readdir    = fs_readdir,
	.read       = fs_read,
	.mkdir      = fs_mkdir,
	.rmdir      = fs_rmdir,
	.unlink     = fs_unlink,
	.rename     = fs_rename,
	.truncate   = fs_truncate,
	.utime      = fs_utime,
	.mknod      = fs_mknod,
	.write      = fs_write,
	.statfs     = fs_statfs,
	.open       = fs_open,
	.release    = fs_release,
	.opendir    = fs_opendir,
	.releasedir = fs_releasedir
};

int main(int argc, char *argv[])
{
	if(disk_init())
		{
		printf("Can't open virtual disk!\n");
		return -1;
		}
	if(mkfs())
		{
		printf("Mkfs failed!\n");
		return -2;
		}
    return fuse_main(argc, argv, &fs_operations, NULL);
}
