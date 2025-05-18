#include "zfs.h"
#include "string.h"
#include <stddef.h>

// ATA硬盘控制器端口
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECTOR_CNT  0x1F2
#define ATA_PRIMARY_SECTOR_LOW  0x1F3
#define ATA_PRIMARY_SECTOR_MID  0x1F4
#define ATA_PRIMARY_SECTOR_HIGH 0x1F5
#define ATA_PRIMARY_DRIVE_HEAD  0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7
#define ATA_PRIMARY_CTRL        0x3F6

// 使用 Primary ATA 控制器
#define ATA_DATA        ATA_PRIMARY_DATA
#define ATA_ERROR       ATA_PRIMARY_ERROR
#define ATA_SECTOR_CNT  ATA_PRIMARY_SECTOR_CNT
#define ATA_SECTOR_LOW  ATA_PRIMARY_SECTOR_LOW
#define ATA_SECTOR_MID  ATA_PRIMARY_SECTOR_MID
#define ATA_SECTOR_HIGH ATA_PRIMARY_SECTOR_HIGH
#define ATA_DRIVE_HEAD  ATA_PRIMARY_DRIVE_HEAD
#define ATA_STATUS      ATA_PRIMARY_STATUS
#define ATA_COMMAND     ATA_PRIMARY_COMMAND
#define ATA_CTRL        ATA_PRIMARY_CTRL

// ATA命令
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_FLUSH   0xE7

// ATA状态
#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01

// 常量
#define SECTORS_PER_BLOCK (ZFS_BLOCK_SIZE / 512)
#define MAX_FD 16
#define WHENCE_SET 0
#define WHENCE_CUR 1
#define WHENCE_END 2

// 全局变量
static zfs_fs_t zfs_fs;
static uint8_t disk_buffer[ZFS_BLOCK_SIZE];

// 从外部导入的函数
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void print_string(const char* str);
extern void print_int(int num);
extern void print_newline(void);

// 临时替代函数
static uint64_t get_current_time(void) {
    static uint64_t ticks = 0;
    return ticks++;
}

// 获取ZFS文件系统实例的函数
zfs_fs_t* get_zfs_fs(void) {
    return &zfs_fs;
}

// IO辅助函数
static void ata_wait_ready(void) {
    while ((inb(ATA_STATUS) & ATA_STATUS_BSY));
}

// 读取一个扇区
static int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    uint8_t status;
    int i;
    
    // 等待驱动器就绪
    ata_wait_ready();
    
    // 选择驱动器并设置LBA模式
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 等待驱动器就绪
    ata_wait_ready();
    
    // 设置参数
    outb(ATA_SECTOR_CNT, 1);             // 读取1个扇区
    outb(ATA_SECTOR_LOW, lba & 0xFF);
    outb(ATA_SECTOR_MID, (lba >> 8) & 0xFF);
    outb(ATA_SECTOR_HIGH, (lba >> 16) & 0xFF);
    
    // 发送读取命令
    outb(ATA_COMMAND, ATA_CMD_READ);
    
    // 等待数据准备就绪
    while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
    
    // 从数据端口读取扇区数据 (512字节)
    for (i = 0; i < 512; i += 2) {
        uint16_t data = inb(ATA_DATA) | (inb(ATA_DATA) << 8);
        buffer[i] = data & 0xFF;
        buffer[i+1] = (data >> 8) & 0xFF;
    }
    
    // 检查错误
    status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR) {
        return -1;
    }
    
    return 0;
}

// 写入一个扇区
static int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    uint8_t status;
    int i;
    
    // 等待驱动器就绪
    ata_wait_ready();
    
    // 选择驱动器并设置LBA模式
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 等待驱动器就绪
    ata_wait_ready();
    
    // 设置参数
    outb(ATA_SECTOR_CNT, 1);             // 写入1个扇区
    outb(ATA_SECTOR_LOW, lba & 0xFF);
    outb(ATA_SECTOR_MID, (lba >> 8) & 0xFF);
    outb(ATA_SECTOR_HIGH, (lba >> 16) & 0xFF);
    
    // 发送写入命令
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    
    // 等待驱动器准备好接收数据
    while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
    
    // 向数据端口写入扇区数据 (512字节)
    for (i = 0; i < 512; i += 2) {
        uint16_t data = buffer[i] | (buffer[i+1] << 8);
        outb(ATA_DATA, data & 0xFF);
        outb(ATA_DATA, (data >> 8) & 0xFF);
    }
    
    // 刷新缓存
    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    
    // 等待操作完成
    ata_wait_ready();
    
    // 检查错误
    status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR) {
        return -1;
    }
    
    return 0;
}

// 读取一个块
int zfs_read_block(zfs_fs_t* fs, uint32_t block, void* buffer) {
    uint32_t start_sector = fs->disk_start_sector + (block * SECTORS_PER_BLOCK);
    uint8_t* buf = (uint8_t*)buffer;
    
    for (uint32_t i = 0; i < SECTORS_PER_BLOCK; i++) {
        if (ata_read_sector(start_sector + i, buf + (i * 512)) != 0) {
            return -1;
        }
    }
    
    return 0;
}

// 写入一个块
int zfs_write_block(zfs_fs_t* fs, uint32_t block, const void* buffer) {
    uint32_t start_sector = fs->disk_start_sector + (block * SECTORS_PER_BLOCK);
    const uint8_t* buf = (const uint8_t*)buffer;
    
    for (uint32_t i = 0; i < SECTORS_PER_BLOCK; i++) {
        if (ata_write_sector(start_sector + i, buf + (i * 512)) != 0) {
            return -1;
        }
    }
    
    return 0;
}

// 工具函数：位图操作
static void bitmap_set(uint8_t* bitmap, uint32_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

static void bitmap_clear(uint8_t* bitmap, uint32_t index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

static int bitmap_test(const uint8_t* bitmap, uint32_t index) {
    return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}

// 分配一个块
int zfs_alloc_block(zfs_fs_t* fs) {
    uint32_t i, j;
    uint32_t bitmap_size = ZFS_BLOCK_SIZE * 8; // 位数
    
    // 确保数据位图已加载
    if (!fs->data_bitmap) {
        fs->data_bitmap = (uint8_t*)0x200000; // 分配内存
        if (zfs_read_block(fs, fs->superblock.data_bitmap_block, fs->data_bitmap) != 0) {
            return -1;
        }
    }
    
    // 查找空闲块
    for (i = 0; i < bitmap_size; i++) {
        if (!bitmap_test(fs->data_bitmap, i)) {
            // 找到空闲块
            bitmap_set(fs->data_bitmap, i);
            
            // 更新数据位图到磁盘
            if (zfs_write_block(fs, fs->superblock.data_bitmap_block, fs->data_bitmap) != 0) {
                return -1;
            }
            
            // 更新超级块
            fs->superblock.free_blocks--;
            if (zfs_write_block(fs, ZFS_SUPERBLOCK_OFFSET, &fs->superblock) != 0) {
                return -1;
            }
            
            return i + fs->superblock.data_blocks_start;
        }
    }
    
    return -1; // 没有空闲块
}

// 释放一个块
int zfs_free_block(zfs_fs_t* fs, uint32_t block) {
    // 确保块号有效
    if (block < fs->superblock.data_blocks_start) {
        return -1;
    }
    
    uint32_t idx = block - fs->superblock.data_blocks_start;
    
    // 确保数据位图已加载
    if (!fs->data_bitmap) {
        fs->data_bitmap = (uint8_t*)0x200000; // 分配内存
        if (zfs_read_block(fs, fs->superblock.data_bitmap_block, fs->data_bitmap) != 0) {
            return -1;
        }
    }
    
    // 清除位图位
    bitmap_clear(fs->data_bitmap, idx);
    
    // 更新数据位图到磁盘
    if (zfs_write_block(fs, fs->superblock.data_bitmap_block, fs->data_bitmap) != 0) {
        return -1;
    }
    
    // 更新超级块
    fs->superblock.free_blocks++;
    if (zfs_write_block(fs, ZFS_SUPERBLOCK_OFFSET, &fs->superblock) != 0) {
        return -1;
    }
    
    return 0;
}

// 分配inode
int zfs_alloc_inode(zfs_fs_t* fs) {
    uint32_t i;
    uint32_t bitmap_size = ZFS_BLOCK_SIZE * 8; // 位数
    
    // 确保inode位图已加载
    if (!fs->inode_bitmap) {
        fs->inode_bitmap = (uint8_t*)0x300000; // 分配内存
        if (zfs_read_block(fs, fs->superblock.inode_bitmap_block, fs->inode_bitmap) != 0) {
            return -1;
        }
    }
    
    // 跳过0号inode
    for (i = 1; i < bitmap_size; i++) {
        if (!bitmap_test(fs->inode_bitmap, i)) {
            // 找到空闲inode
            bitmap_set(fs->inode_bitmap, i);
            
            // 更新inode位图到磁盘
            if (zfs_write_block(fs, fs->superblock.inode_bitmap_block, fs->inode_bitmap) != 0) {
                return -1;
            }
            
            return i;
        }
    }
    
    return -1; // 没有空闲inode
}

// 释放inode
int zfs_free_inode(zfs_fs_t* fs, uint32_t inode) {
    // 确保inode号有效
    if (inode < 1) {
        return -1;
    }
    
    // 确保inode位图已加载
    if (!fs->inode_bitmap) {
        fs->inode_bitmap = (uint8_t*)0x300000; // 分配内存
        if (zfs_read_block(fs, fs->superblock.inode_bitmap_block, fs->inode_bitmap) != 0) {
            return -1;
        }
    }
    
    // 清除位图位
    bitmap_clear(fs->inode_bitmap, inode);
    
    // 更新inode位图到磁盘
    if (zfs_write_block(fs, fs->superblock.inode_bitmap_block, fs->inode_bitmap) != 0) {
        return -1;
    }
    
    return 0;
}

// 读取inode
static int read_inode(zfs_fs_t* fs, uint32_t inode, zfs_inode_t* inode_data) {
    if (inode < 1) {
        return -1;
    }
    
    uint32_t inodes_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_inode_t);
    uint32_t block = fs->superblock.inode_table_block + (inode / inodes_per_block);
    uint32_t offset = inode % inodes_per_block;
    
    // 读取包含指定inode的块
    if (zfs_read_block(fs, block, disk_buffer) != 0) {
        return -1;
    }
    
    // 复制inode数据
    memcpy(inode_data, disk_buffer + (offset * sizeof(zfs_inode_t)), sizeof(zfs_inode_t));
    
    return 0;
}

// 写入inode
static int write_inode(zfs_fs_t* fs, uint32_t inode, const zfs_inode_t* inode_data) {
    if (inode < 1) {
        return -1;
    }
    
    uint32_t inodes_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_inode_t);
    uint32_t block = fs->superblock.inode_table_block + (inode / inodes_per_block);
    uint32_t offset = inode % inodes_per_block;
    
    // 读取包含指定inode的块
    if (zfs_read_block(fs, block, disk_buffer) != 0) {
        return -1;
    }
    
    // 更新inode数据
    memcpy(disk_buffer + (offset * sizeof(zfs_inode_t)), inode_data, sizeof(zfs_inode_t));
    
    // 写回块
    if (zfs_write_block(fs, block, disk_buffer) != 0) {
        return -1;
    }
    
    return 0;
}

// 初始化ZFS文件系统
int zfs_init(zfs_fs_t* fs, uint32_t disk_sector) {
    // 设置磁盘起始扇区
    fs->disk_start_sector = disk_sector;
    
    // 读取超级块
    if (zfs_read_block(fs, ZFS_SUPERBLOCK_OFFSET, &fs->superblock) != 0) {
        return -1;
    }
    
    // 验证ZFS签名
    if (fs->superblock.magic != ZFS_MAGIC) {
        return -2; // 不是ZFS分区
    }
    
    // 初始化成功
    fs->mounted = 0;
    fs->inode_bitmap = NULL;
    fs->data_bitmap = NULL;
    
    // 初始化文件句柄
    for (int i = 0; i < MAX_FD; i++) {
        fs->file_handles[i].is_open = 0;
    }
    
    return 0;
}

// 挂载ZFS文件系统
int zfs_mount(zfs_fs_t* fs) {
    if (fs->mounted) {
        return 0; // 已经挂载
    }
    
    // 分配内存
    fs->inode_bitmap = (uint8_t*)0x300000; // 为inode位图分配内存
    fs->data_bitmap = (uint8_t*)0x200000;  // 为数据位图分配内存
    
    // 加载位图
    if (zfs_read_block(fs, fs->superblock.inode_bitmap_block, fs->inode_bitmap) != 0) {
        return -1;
    }
    
    if (zfs_read_block(fs, fs->superblock.data_bitmap_block, fs->data_bitmap) != 0) {
        return -1;
    }
    
    // 更新挂载时间
    fs->superblock.mount_time = get_current_time();
    
    // 更新超级块
    if (zfs_write_block(fs, ZFS_SUPERBLOCK_OFFSET, &fs->superblock) != 0) {
        return -1;
    }
    
    // 标记为已挂载
    fs->mounted = 1;
    
    return 0;
}

// 卸载ZFS文件系统
int zfs_unmount(zfs_fs_t* fs) {
    if (!fs->mounted) {
        return 0; // 已经卸载
    }
    
    // 更新超级块
    if (zfs_write_block(fs, ZFS_SUPERBLOCK_OFFSET, &fs->superblock) != 0) {
        return -1;
    }
    
    // 关闭所有打开的文件
    for (int i = 0; i < MAX_FD; i++) {
        if (fs->file_handles[i].is_open) {
            fs->file_handles[i].is_open = 0;
        }
    }
    
    // 标记为已卸载
    fs->mounted = 0;
    
    return 0;
}

// 初始化目录
static int init_directory(zfs_fs_t* fs, uint32_t dir_inode, uint32_t parent_inode) {
    uint32_t block;
    zfs_direntry_t* entry;
    zfs_inode_t inode;
    
    // 读取目录的inode
    if (read_inode(fs, dir_inode, &inode) != 0) {
        return -1;
    }
    
    // 分配一个块用于目录内容
    block = zfs_alloc_block(fs);
    if (block == (uint32_t)-1) {
        return -1;
    }
    
    // 更新inode
    inode.direct_blocks[0] = block;
    inode.size = 2 * sizeof(zfs_direntry_t); // . 和 ..
    inode.mode = ZFS_TYPE_DIRECTORY | ZFS_PERM_READ | ZFS_PERM_WRITE | ZFS_PERM_EXEC;
    inode.ctime = inode.mtime = inode.atime = get_current_time();
    
    // 写回inode
    if (write_inode(fs, dir_inode, &inode) != 0) {
        zfs_free_block(fs, block);
        return -1;
    }
    
    // 清空块
    memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
    
    // 添加 . 条目
    entry = (zfs_direntry_t*)disk_buffer;
    entry->inode = dir_inode;
    entry->file_type = ZFS_TYPE_DIRECTORY;
    entry->name_len = 1;
    entry->name[0] = '.';
    entry->name[1] = '\0';
    
    // 添加 .. 条目
    entry = (zfs_direntry_t*)(disk_buffer + sizeof(zfs_direntry_t));
    entry->inode = parent_inode;
    entry->file_type = ZFS_TYPE_DIRECTORY;
    entry->name_len = 2;
    entry->name[0] = '.';
    entry->name[1] = '.';
    entry->name[2] = '\0';
    
    // 写入目录内容
    if (zfs_write_block(fs, block, disk_buffer) != 0) {
        return -1;
    }
    
    return 0;
}

// 格式化ZFS分区
int zfs_format(uint32_t disk_sector, uint64_t size, const char* label) {
    zfs_fs_t fs;
    zfs_superblock_t* sb = &fs.superblock;
    uint32_t blocks_count, i;
    
    // 简单初始化
    fs.disk_start_sector = disk_sector;
    
    // 计算块数量
    blocks_count = size / ZFS_BLOCK_SIZE;
    
    // 初始化超级块
    memset(sb, 0, sizeof(zfs_superblock_t));
    sb->magic = ZFS_MAGIC;
    sb->version = 1;
    sb->block_size = ZFS_BLOCK_SIZE;
    sb->total_blocks = blocks_count;
    sb->free_blocks = blocks_count - ZFS_RESERVED_BLOCKS - ZFS_INODE_BLOCKS - 2; // 减去超级块、位图块和inode表
    sb->root_inode = ZFS_ROOT_INODE;
    
    // 复制卷标
    if (label) {
        int len = strlen(label);
        if (len > 15) len = 15;
        memcpy(sb->label, label, len);
        sb->label[len] = '\0';
    } else {
        strcpy((char*)sb->label, "ZZQ-ZFS");
    }
    
    // 设置分区布局
    sb->inode_bitmap_block = 1;
    sb->data_bitmap_block = 2;
    sb->inode_table_block = 3;
    sb->data_blocks_start = 3 + ZFS_INODE_BLOCKS;
    
    // 设置时间戳
    sb->create_time = sb->mount_time = get_current_time();
    
    // 写入超级块
    if (zfs_write_block(&fs, ZFS_SUPERBLOCK_OFFSET, sb) != 0) {
        print_string("写入超级块失败");
        print_newline();
        return -1;
    }
    
    // 创建并初始化inode位图
    memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
    bitmap_set(disk_buffer, ZFS_ROOT_INODE); // 设置根目录inode为已使用
    if (zfs_write_block(&fs, sb->inode_bitmap_block, disk_buffer) != 0) {
        print_string("写入inode位图失败");
        print_newline();
        return -1;
    }
    
    // 创建并初始化数据位图
    memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
    // 标记保留块为已使用
    for (i = 0; i < sb->data_blocks_start; i++) {
        bitmap_set(disk_buffer, i);
    }
    if (zfs_write_block(&fs, sb->data_bitmap_block, disk_buffer) != 0) {
        print_string("写入数据位图失败");
        print_newline();
        return -1;
    }
    
    // 初始化inode表
    memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
    if (zfs_write_block(&fs, sb->inode_table_block, disk_buffer) != 0) {
        print_string("写入inode表失败");
        print_newline();
        return -1;
    }
    
    // 初始化根目录
    if (init_directory(&fs, ZFS_ROOT_INODE, ZFS_ROOT_INODE) != 0) {
        print_string("初始化根目录失败");
        print_newline();
        return -1;
    }
    
    print_string("ZFS格式化成功，总块数: ");
    print_int(blocks_count);
    print_newline();
    
    return 0;
} 