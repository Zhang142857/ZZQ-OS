#ifndef ZFS_H
#define ZFS_H

#include <stdint.h>

// ZFS - ZZQ File System 结构定义

// 魔数 "ZFS1"
#define ZFS_MAGIC 0x5A465331

// 文件系统常量
#define ZFS_BLOCK_SIZE          4096    // 基本块大小 (4KB)
#define ZFS_SUPERBLOCK_OFFSET   0       // 超级块位置
#define ZFS_MAX_FILENAME        64      // 最大文件名长度
#define ZFS_ROOT_INODE          1       // 根目录inode号
#define ZFS_MAX_FILE_SIZE       (16*1024*1024) // 最大文件大小 (16MB)
#define ZFS_RESERVED_BLOCKS     16      // 保留块数量
#define ZFS_INODE_BLOCKS        64      // 默认inode块数量

// 文件类型
#define ZFS_TYPE_FILE           1       // 普通文件
#define ZFS_TYPE_DIRECTORY      2       // 目录
#define ZFS_TYPE_SYMLINK        3       // 符号链接

// 权限标志
#define ZFS_PERM_READ           0x04    // 读权限
#define ZFS_PERM_WRITE          0x02    // 写权限
#define ZFS_PERM_EXEC           0x01    // 执行权限

// 超级块结构
typedef struct {
    uint32_t magic;                 // 文件系统魔数 (ZFS_MAGIC)
    uint32_t version;               // 文件系统版本
    uint32_t block_size;            // 块大小
    uint32_t total_blocks;          // 总块数
    uint32_t free_blocks;           // 空闲块数
    uint32_t inode_bitmap_block;    // inode位图块号
    uint32_t data_bitmap_block;     // 数据位图块号
    uint32_t inode_table_block;     // inode表起始块号
    uint32_t data_blocks_start;     // 数据区起始块号
    uint32_t root_inode;            // 根目录inode号
    uint64_t mount_time;            // 挂载时间
    uint64_t create_time;           // 创建时间
    uint8_t  label[16];             // 卷标
    uint8_t  reserved[64];          // 保留字节
} __attribute__((packed)) zfs_superblock_t;

// inode结构
typedef struct {
    uint32_t mode;                  // 文件类型和权限
    uint32_t uid;                   // 所有者ID
    uint32_t gid;                   // 组ID
    uint32_t size;                  // 文件大小
    uint64_t atime;                 // 最后访问时间
    uint64_t mtime;                 // 最后修改时间
    uint64_t ctime;                 // 创建时间
    uint32_t direct_blocks[10];     // 直接数据块指针
    uint32_t indirect_block;        // 一级间接块指针
    uint32_t double_indirect_block; // 二级间接块指针
    uint8_t  reserved[32];          // 保留字节
} __attribute__((packed)) zfs_inode_t;

// 目录项结构
typedef struct {
    uint32_t inode;                 // inode号
    uint8_t  name_len;              // 文件名长度
    uint8_t  file_type;             // 文件类型
    char     name[ZFS_MAX_FILENAME];// 文件名
} __attribute__((packed)) zfs_direntry_t;

// 文件描述符
typedef struct {
    uint32_t inode;                 // inode号
    uint32_t position;              // 文件指针位置
    uint32_t mode;                  // 打开模式
    uint8_t  is_open;               // 是否打开
} zfs_fd_t;

// ZFS文件系统实例
typedef struct {
    zfs_superblock_t superblock;    // 超级块缓存
    uint8_t* inode_bitmap;          // inode位图
    uint8_t* data_bitmap;           // 数据位图
    uint32_t disk_start_sector;     // 磁盘起始扇区
    uint8_t  mounted;               // 是否已挂载
    zfs_fd_t file_handles[16];      // 文件句柄表
} zfs_fs_t;

// 函数声明

// 文件系统操作
zfs_fs_t* get_zfs_fs(void);
int zfs_init(zfs_fs_t* fs, uint32_t disk_sector);
int zfs_format(uint32_t disk_sector, uint64_t size, const char* label);
int zfs_mount(zfs_fs_t* fs);
int zfs_unmount(zfs_fs_t* fs);

// 文件/目录操作
int zfs_open(zfs_fs_t* fs, const char* path, int mode);
int zfs_close(zfs_fs_t* fs, int fd);
int zfs_read(zfs_fs_t* fs, int fd, void* buffer, uint32_t size);
int zfs_write(zfs_fs_t* fs, int fd, const void* buffer, uint32_t size);
int zfs_seek(zfs_fs_t* fs, int fd, int offset, int whence);
int zfs_create(zfs_fs_t* fs, const char* path, int mode);
int zfs_mkdir(zfs_fs_t* fs, const char* path, int mode);
int zfs_unlink(zfs_fs_t* fs, const char* path);
int zfs_rmdir(zfs_fs_t* fs, const char* path);
int zfs_readdir(zfs_fs_t* fs, const char* path, zfs_direntry_t* entries, int count);

// 底层块操作
int zfs_read_block(zfs_fs_t* fs, uint32_t block, void* buffer);
int zfs_write_block(zfs_fs_t* fs, uint32_t block, const void* buffer);
int zfs_alloc_block(zfs_fs_t* fs);
int zfs_free_block(zfs_fs_t* fs, uint32_t block);
int zfs_alloc_inode(zfs_fs_t* fs);
int zfs_free_inode(zfs_fs_t* fs, uint32_t inode);

#endif /* ZFS_H */ 