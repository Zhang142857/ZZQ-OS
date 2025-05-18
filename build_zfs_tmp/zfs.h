#ifndef ZFS_H
#define ZFS_H

#include <stdint.h>

// ZFS 文件系统常量定义
#define ZFS_MAGIC              0x5A465300  // "ZFS\0"
#define ZFS_VERSION            0x0100      // 版本 1.0
#define ZFS_BLOCK_SIZE         512         // 块大小 (字节)
#define ZFS_NAME_LENGTH        32          // 最大文件名长度
#define ZFS_MAX_FILE_SIZE      (1024*1024) // 最大文件大小 1MB
#define ZFS_MAX_FILES          64          // 最大文件数量
#define ZFS_RESERVED_BLOCKS    16          // 保留块数量
#define ZFS_INVALID_BLOCK      0xFFFFFFFF  // 无效块标记

// ZFS 文件属性
#define ZFS_ATTR_DIRECTORY     0x01        // 目录标志
#define ZFS_ATTR_SYSTEM        0x02        // 系统文件标志
#define ZFS_ATTR_HIDDEN        0x04        // 隐藏文件标志
#define ZFS_ATTR_READONLY      0x08        // 只读文件标志

// ZFS 返回值
#define ZFS_OK                 0           // 操作成功
#define ZFS_ERROR              -1          // 一般错误
#define ZFS_ERR_NOT_MOUNTED    -2          // 文件系统未挂载
#define ZFS_ERR_INVALID_FS     -3          // 无效的文件系统
#define ZFS_ERR_NO_SPACE       -4          // 没有可用空间
#define ZFS_ERR_FILE_NOT_FOUND -5          // 文件未找到
#define ZFS_ERR_FILE_EXISTS    -6          // 文件已存在
#define ZFS_ERR_TOO_LARGE      -7          // 文件过大
#define ZFS_ERR_READONLY       -8          // 文件只读

// ZFS 超级块结构
typedef struct {
    uint32_t magic;                        // 文件系统魔数
    uint16_t version;                      // 文件系统版本
    uint16_t block_size;                   // 块大小 (字节)
    uint32_t total_blocks;                 // 总块数
    uint32_t bitmap_block;                 // 位图块开始位置
    uint32_t bitmap_blocks;                // 位图块数量
    uint32_t inode_table_block;            // inode表开始位置
    uint32_t inode_table_blocks;           // inode表块数量
    uint32_t data_block;                   // 数据区开始位置
    uint32_t root_inode;                   // 根目录inode号
    uint32_t free_blocks;                  // 剩余可用块数量
    uint32_t free_inodes;                  // 剩余可用inode数量
    uint8_t label[16];                     // 卷标
    uint8_t reserved[32];                  // 保留字段
} zfs_superblock_t;

// ZFS inode结构
typedef struct {
    uint32_t inode_num;                    // inode号
    uint8_t filename[ZFS_NAME_LENGTH];     // 文件名
    uint8_t attributes;                    // 文件属性
    uint32_t size;                         // 文件大小 (字节)
    uint32_t create_time;                  // 创建时间 (UNIX时间戳)
    uint32_t modify_time;                  // 修改时间 (UNIX时间戳)
    uint32_t access_time;                  // 访问时间 (UNIX时间戳)
    uint32_t direct_blocks[10];            // 直接块指针
    uint32_t indirect_block;               // 间接块指针
    uint8_t reserved[16];                  // 保留字段
} zfs_inode_t;

// ZFS 目录项结构
typedef struct {
    uint32_t inode_num;                    // inode号
    uint8_t filename[ZFS_NAME_LENGTH];     // 文件名
    uint8_t attributes;                    // 文件属性
} zfs_direntry_t;

// ZFS 文件系统结构
typedef struct {
    uint8_t mounted;                       // 挂载标志
    uint32_t disk_sector;                  // 磁盘起始扇区
    zfs_superblock_t superblock;           // 超级块
    uint8_t* bitmap;                       // 位图缓存
    zfs_inode_t* current_inode;            // 当前操作的inode
    uint32_t current_inode_num;            // 当前inode号
    uint8_t* cache;                        // 数据缓存
    uint32_t cache_block;                  // 当前缓存的块号
    uint8_t cache_dirty;                   // 缓存脏标志
} zfs_fs_t;

// ZFS 文件描述符
typedef struct {
    uint32_t inode_num;                    // inode号
    uint32_t position;                     // 文件位置
    uint8_t mode;                          // 访问模式
} zfs_file_t;

// ZFS 操作函数

// 获取ZFS文件系统实例
zfs_fs_t* get_zfs_fs(void);

// 初始化ZFS文件系统
int zfs_init(zfs_fs_t* fs, uint32_t disk_sector);

// 格式化ZFS文件系统
int zfs_format(uint32_t disk_sector, uint32_t size);

// 挂载ZFS文件系统
int zfs_mount(zfs_fs_t* fs);

// 卸载ZFS文件系统
int zfs_unmount(zfs_fs_t* fs);

// 同步文件系统 (写回缓存)
int zfs_sync(zfs_fs_t* fs);

// 创建文件或目录
int zfs_create(zfs_fs_t* fs, const char* path, uint8_t attributes);

// 删除文件或目录
int zfs_delete(zfs_fs_t* fs, const char* path);

// 打开文件
int zfs_open(zfs_fs_t* fs, const char* path, uint8_t mode, zfs_file_t* file);

// 关闭文件
int zfs_close(zfs_fs_t* fs, zfs_file_t* file);

// 读取文件
int zfs_read(zfs_fs_t* fs, zfs_file_t* file, void* buffer, uint32_t size);

// 写入文件
int zfs_write(zfs_fs_t* fs, zfs_file_t* file, const void* buffer, uint32_t size);

// 移动文件指针
int zfs_seek(zfs_fs_t* fs, zfs_file_t* file, uint32_t offset);

// 获取文件信息
int zfs_stat(zfs_fs_t* fs, const char* path, zfs_inode_t* inode);

// 列出目录内容
int zfs_list_directory(zfs_fs_t* fs, const char* path, zfs_direntry_t* entries, uint32_t* count);

// 重命名文件或目录
int zfs_rename(zfs_fs_t* fs, const char* old_path, const char* new_path);

// 调试函数
void zfs_dump_info(zfs_fs_t* fs);

#endif // ZFS_H 