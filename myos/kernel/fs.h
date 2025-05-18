#ifndef FS_H
#define FS_H

// 文件系统常量定义
#define MAX_FILENAME_LENGTH 32
#define MAX_FILES 16  // 减少为16个文件，确保文件表不超过一个扇区
#define MAX_FILE_SIZE 4096
#define SECTOR_SIZE 512

// 文件属性
#define FILE_ATTR_NORMAL 0x00
#define FILE_ATTR_DIRECTORY 0x01
#define FILE_ATTR_SYSTEM 0x02
#define FILE_ATTR_READONLY 0x04

// 文件条目结构
typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    unsigned int size;
    unsigned char attributes;
    unsigned char is_used;
    unsigned int start_sector;  // 文件数据的起始扇区
} file_entry_t;

// 文件系统结构
typedef struct {
    unsigned int num_sectors;   // 文件系统占用的扇区数
    unsigned int data_start;    // 数据区域的起始扇区
    file_entry_t file_table[MAX_FILES];
    unsigned int free_sector;   // 下一个可用的数据扇区
} filesystem_t;

// 文件句柄结构
typedef struct {
    int file_index;       // 文件表中的索引
    unsigned int position; // 当前文件位置
} file_t;

// 文件系统状态访问
void fs_set_initialized(int value);
void fs_set_mounted(int value);
int fs_is_initialized();
int fs_is_mounted();

// 持久化存储控制
void fs_enable_persistence();

// 文件系统初始化与挂载
void fs_init();
int fs_format();
int fs_mount();

// 文件操作
file_t fs_open(const char* filename, int create);
int fs_close(file_t* file);
int fs_read(file_t* file, void* buffer, unsigned int size);
int fs_write(file_t* file, const void* buffer, unsigned int size);
int fs_delete(const char* filename);
int fs_rename(const char* oldname, const char* newname);
int fs_seek(file_t* file, unsigned int position);

// 目录操作
int fs_list_files(char* buffer, unsigned int buffer_size);
unsigned int fs_get_file_size(const char* filename);
int fs_file_exists(const char* filename);

// 磁盘操作
int disk_read_sector(unsigned int sector, void* buffer);
int disk_write_sector(unsigned int sector, const void* buffer);
int disk_flush();

#endif // FS_H 