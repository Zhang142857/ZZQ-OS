#include "zfs.h"
#include "string.h"

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

// 全局变量
static zfs_fs_t zfs_fs;
static uint8_t disk_buffer[512];
static uint8_t bitmap_cache[8192]; // 支持最大64K个块的位图缓存
static zfs_inode_t inode_cache;

// 从外部导入的函数
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void print_string(const char* str);
extern void print_int(int num);
extern void print_newline(void);
extern unsigned int get_tick(void);

// 获取ZFS文件系统实例
zfs_fs_t* get_zfs_fs(void) {
    return &zfs_fs;
}

// 读取一个扇区
static int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    // 复位ATA控制器
    outb(ATA_CTRL, 0x04);
    outb(ATA_CTRL, 0x00);
    
    // 等待驱动器就绪
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
    
    // 选择驱动器并设置LBA模式
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 等待驱动器就绪
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
    
    // 设置参数
    outb(ATA_SECTOR_CNT, 1); // 读取1个扇区
    outb(ATA_SECTOR_LOW, lba & 0xFF);
    outb(ATA_SECTOR_MID, (lba >> 8) & 0xFF);
    outb(ATA_SECTOR_HIGH, (lba >> 16) & 0xFF);
    
    // 发送读取命令
    outb(ATA_COMMAND, ATA_CMD_READ);
    
    // 等待数据准备就绪
    while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
    
    // 从数据端口读取扇区数据 (512字节)
    for (int i = 0; i < 512; i += 2) {
        // 读取16位数据
        uint16_t data = inb(ATA_DATA) | (inb(ATA_DATA) << 8);
        buffer[i] = data & 0xFF;
        buffer[i+1] = (data >> 8) & 0xFF;
    }
    
    // 检查错误
    uint8_t status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 写入一个扇区
static int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    // 复位ATA控制器
    outb(ATA_CTRL, 0x04);
    outb(ATA_CTRL, 0x00);
    
    // 等待驱动器就绪
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
    
    // 选择驱动器并设置LBA模式
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 等待驱动器就绪
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
    
    // 设置参数
    outb(ATA_SECTOR_CNT, 1); // 写入1个扇区
    outb(ATA_SECTOR_LOW, lba & 0xFF);
    outb(ATA_SECTOR_MID, (lba >> 8) & 0xFF);
    outb(ATA_SECTOR_HIGH, (lba >> 16) & 0xFF);
    
    // 发送写入命令
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    
    // 等待驱动器准备好接收数据
    while (!(inb(ATA_STATUS) & ATA_STATUS_DRQ));
    
    // 向数据端口写入扇区数据 (512字节)
    for (int i = 0; i < 512; i += 2) {
        uint16_t data = buffer[i] | (buffer[i+1] << 8);
        // 使用16位数据传输
        outb(ATA_DATA, data & 0xFF);
        outb(ATA_DATA, (data >> 8) & 0xFF);
    }
    
    // 等待操作完成
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
    
    // 检查错误
    uint8_t status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 读取一个块
static int read_block(zfs_fs_t* fs, uint32_t block_num, uint8_t* buffer) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (block_num >= fs->superblock.total_blocks) {
        return ZFS_ERROR;
    }
    
    uint32_t lba = fs->disk_sector + block_num;
    return ata_read_sector(lba, buffer);
}

// 写入一个块
static int write_block(zfs_fs_t* fs, uint32_t block_num, const uint8_t* buffer) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (block_num >= fs->superblock.total_blocks) {
        return ZFS_ERROR;
    }
    
    uint32_t lba = fs->disk_sector + block_num;
    return ata_write_sector(lba, buffer);
}

// 分配一个块
static int allocate_block(zfs_fs_t* fs) {
    if (!fs->mounted || !fs->bitmap) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (fs->superblock.free_blocks == 0) {
        return ZFS_ERR_NO_SPACE;
    }
    
    // 搜索位图找到第一个空闲块
    uint32_t bitmap_size = fs->superblock.bitmap_blocks * ZFS_BLOCK_SIZE;
    for (uint32_t i = 0; i < bitmap_size; i++) {
        uint8_t mask = 1;
        for (uint8_t j = 0; j < 8; j++) {
            if (!(fs->bitmap[i] & mask)) {
                // 找到空闲块, 标记为已用
                fs->bitmap[i] |= mask;
                fs->superblock.free_blocks--;
                
                // 计算块号
                uint32_t block_num = i * 8 + j + fs->superblock.data_block;
                
                // 标记位图为脏
                fs->cache_dirty = 1;
                
                return block_num;
            }
            mask <<= 1;
        }
    }
    
    return ZFS_ERR_NO_SPACE;
}

// 释放一个块
static int free_block(zfs_fs_t* fs, uint32_t block_num) {
    if (!fs->mounted || !fs->bitmap) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (block_num < fs->superblock.data_block || 
        block_num >= fs->superblock.total_blocks) {
        return ZFS_ERROR;
    }
    
    // 计算位图索引和位掩码
    uint32_t bitmap_index = (block_num - fs->superblock.data_block) / 8;
    uint8_t bit_index = (block_num - fs->superblock.data_block) % 8;
    uint8_t mask = 1 << bit_index;
    
    // 确保块已分配
    if (!(fs->bitmap[bitmap_index] & mask)) {
        return ZFS_ERROR; // 块已经是空闲的
    }
    
    // 标记块为空闲
    fs->bitmap[bitmap_index] &= ~mask;
    fs->superblock.free_blocks++;
    
    // 标记位图为脏
    fs->cache_dirty = 1;
    
    return ZFS_OK;
}

// 读取inode
static int read_inode(zfs_fs_t* fs, uint32_t inode_num, zfs_inode_t* inode) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (inode_num >= ZFS_MAX_FILES) {
        return ZFS_ERROR;
    }
    
    // 计算inode所在块和偏移量
    uint32_t inodes_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_inode_t);
    uint32_t block_num = fs->superblock.inode_table_block + inode_num / inodes_per_block;
    uint32_t offset = (inode_num % inodes_per_block) * sizeof(zfs_inode_t);
    
    // 读取块
    if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 复制inode
    memcpy(inode, disk_buffer + offset, sizeof(zfs_inode_t));
    
    // 验证inode是否有效
    if (inode->inode_num != inode_num) {
        return ZFS_ERROR; // 无效的inode
    }
    
    return ZFS_OK;
}

// 写入inode
static int write_inode(zfs_fs_t* fs, const zfs_inode_t* inode) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    uint32_t inode_num = inode->inode_num;
    if (inode_num >= ZFS_MAX_FILES) {
        return ZFS_ERROR;
    }
    
    // 计算inode所在块和偏移量
    uint32_t inodes_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_inode_t);
    uint32_t block_num = fs->superblock.inode_table_block + inode_num / inodes_per_block;
    uint32_t offset = (inode_num % inodes_per_block) * sizeof(zfs_inode_t);
    
    // 读取块
    if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 复制inode
    memcpy(disk_buffer + offset, inode, sizeof(zfs_inode_t));
    
    // 写回块
    if (write_block(fs, block_num, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 分配一个inode
static int allocate_inode(zfs_fs_t* fs) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (fs->superblock.free_inodes == 0) {
        return ZFS_ERR_NO_SPACE;
    }
    
    // 遍历inode表寻找空闲inode
    for (uint32_t i = 0; i < ZFS_MAX_FILES; i++) {
        if (read_inode(fs, i, &inode_cache) != ZFS_OK) {
            continue;
        }
        
        if (inode_cache.inode_num == ZFS_INVALID_BLOCK) {
            // 找到空闲inode
            fs->superblock.free_inodes--;
            return i;
        }
    }
    
    return ZFS_ERR_NO_SPACE;
}

// 释放一个inode
static int free_inode(zfs_fs_t* fs, uint32_t inode_num) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    if (inode_num >= ZFS_MAX_FILES) {
        return ZFS_ERROR;
    }
    
    // 读取inode
    if (read_inode(fs, inode_num, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 释放所有关联的块
    for (int i = 0; i < 10; i++) {
        if (inode_cache.direct_blocks[i] != ZFS_INVALID_BLOCK) {
            free_block(fs, inode_cache.direct_blocks[i]);
            inode_cache.direct_blocks[i] = ZFS_INVALID_BLOCK;
        }
    }
    
    // 处理间接块 (简化版只实现直接块)
    
    // 标记inode为未使用
    inode_cache.inode_num = ZFS_INVALID_BLOCK;
    inode_cache.size = 0;
    
    // 写回inode
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 更新超级块
    fs->superblock.free_inodes++;
    
    return ZFS_OK;
}

// 初始化ZFS文件系统
int zfs_init(zfs_fs_t* fs, uint32_t disk_sector) {
    // 读取超级块
    if (ata_read_sector(disk_sector, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 验证魔数
    zfs_superblock_t* sb = (zfs_superblock_t*)disk_buffer;
    if (sb->magic != ZFS_MAGIC) {
        return ZFS_ERR_INVALID_FS;
    }
    
    // 复制超级块
    memcpy(&fs->superblock, sb, sizeof(zfs_superblock_t));
    
    // 初始化文件系统结构
    fs->mounted = 0;
    fs->disk_sector = disk_sector;
    fs->bitmap = bitmap_cache;
    fs->current_inode = &inode_cache;
    fs->current_inode_num = ZFS_INVALID_BLOCK;
    fs->cache = disk_buffer;
    fs->cache_block = ZFS_INVALID_BLOCK;
    fs->cache_dirty = 0;
    
    return ZFS_OK;
}

// 格式化ZFS文件系统
int zfs_format(uint32_t disk_sector, uint32_t size) {
    // 计算参数
    uint32_t total_blocks = size / ZFS_BLOCK_SIZE;
    uint32_t bitmap_blocks = (total_blocks + 4095) / 4096; // 每块可索引4096个块
    uint32_t inode_blocks = (ZFS_MAX_FILES * sizeof(zfs_inode_t) + ZFS_BLOCK_SIZE - 1) / ZFS_BLOCK_SIZE;
    
    // 构建超级块
    zfs_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    
    sb.magic = ZFS_MAGIC;
    sb.version = ZFS_VERSION;
    sb.block_size = ZFS_BLOCK_SIZE;
    sb.total_blocks = total_blocks;
    sb.bitmap_block = 1; // 从1开始, 0是超级块
    sb.bitmap_blocks = bitmap_blocks;
    sb.inode_table_block = 1 + bitmap_blocks;
    sb.inode_table_blocks = inode_blocks;
    sb.data_block = 1 + bitmap_blocks + inode_blocks;
    sb.root_inode = 0; // 根目录是第一个inode
    sb.free_blocks = total_blocks - 1 - bitmap_blocks - inode_blocks;
    sb.free_inodes = ZFS_MAX_FILES - 1; // 减1是因为根目录会占用一个
    
    // 卷标
    memcpy(sb.label, "ZZQ-DISK", 8);
    
    // 写入超级块
    memcpy(disk_buffer, &sb, sizeof(sb));
    if (ata_write_sector(disk_sector, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 初始化位图
    memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
    
    // 标记保留块为已用
    uint32_t reserved_blocks = 1 + bitmap_blocks + inode_blocks;
    for (uint32_t i = 0; i < reserved_blocks; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        disk_buffer[byte_index] |= (1 << bit_index);
    }
    
    // 写入位图
    for (uint32_t i = 0; i < bitmap_blocks; i++) {
        uint32_t lba = disk_sector + sb.bitmap_block + i;
        if (i == 0) {
            if (ata_write_sector(lba, disk_buffer) != ZFS_OK) {
                return ZFS_ERROR;
            }
        } else {
            // 后续位图块全部初始化为0 (空闲)
            memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
            if (ata_write_sector(lba, disk_buffer) != ZFS_OK) {
                return ZFS_ERROR;
            }
        }
    }
    
    // 初始化inode表
    memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
    
    // 初始化根目录inode
    zfs_inode_t* root_inode = (zfs_inode_t*)disk_buffer;
    root_inode->inode_num = 0;
    memcpy(root_inode->filename, "/", 2);
    root_inode->attributes = ZFS_ATTR_DIRECTORY;
    root_inode->size = 0;
    root_inode->create_time = get_tick();
    root_inode->modify_time = root_inode->create_time;
    root_inode->access_time = root_inode->create_time;
    
    // 所有其他的inode标记为无效
    for (uint32_t i = 1; i < ZFS_BLOCK_SIZE / sizeof(zfs_inode_t); i++) {
        zfs_inode_t* inode = (zfs_inode_t*)(disk_buffer + i * sizeof(zfs_inode_t));
        inode->inode_num = ZFS_INVALID_BLOCK;
        for (int j = 0; j < 10; j++) {
            inode->direct_blocks[j] = ZFS_INVALID_BLOCK;
        }
        inode->indirect_block = ZFS_INVALID_BLOCK;
    }
    
    // 写入inode表
    uint32_t lba = disk_sector + sb.inode_table_block;
    if (ata_write_sector(lba, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 对于其余inode块，初始化所有inode为无效
    memset(disk_buffer, 0xFF, ZFS_BLOCK_SIZE);
    for (uint32_t i = 1; i < inode_blocks; i++) {
        lba = disk_sector + sb.inode_table_block + i;
        if (ata_write_sector(lba, disk_buffer) != ZFS_OK) {
            return ZFS_ERROR;
        }
    }
    
    print_string("ZFS 文件系统格式化成功, 总块数: ");
    print_int(total_blocks);
    print_string(", 可用数据块: ");
    print_int(sb.free_blocks);
    print_newline();
    
    return ZFS_OK;
}

// 挂载ZFS文件系统
int zfs_mount(zfs_fs_t* fs) {
    if (fs->mounted) {
        return ZFS_OK; // 已经挂载
    }
    
    // 加载位图
    for (uint32_t i = 0; i < fs->superblock.bitmap_blocks && i < 16; i++) {
        // 最多加载16个位图块 (支持64K个块)
        if (read_block(fs, fs->superblock.bitmap_block + i,
                      fs->bitmap + i * ZFS_BLOCK_SIZE) != ZFS_OK) {
            return ZFS_ERROR;
        }
    }
    
    // 标记为已挂载
    fs->mounted = 1;
    
    print_string("ZFS 文件系统挂载成功, 卷标: ");
    for (int i = 0; i < 16 && fs->superblock.label[i]; i++) {
        print_char(fs->superblock.label[i]);
    }
    print_newline();
    
    return ZFS_OK;
}

// 卸载ZFS文件系统
int zfs_unmount(zfs_fs_t* fs) {
    if (!fs->mounted) {
        return ZFS_OK; // 已经卸载
    }
    
    // 同步缓存
    if (fs->cache_dirty) {
        // 写回位图
        for (uint32_t i = 0; i < fs->superblock.bitmap_blocks && i < 16; i++) {
            if (write_block(fs, fs->superblock.bitmap_block + i,
                           fs->bitmap + i * ZFS_BLOCK_SIZE) != ZFS_OK) {
                return ZFS_ERROR;
            }
        }
        
        // 写回超级块
        memcpy(disk_buffer, &fs->superblock, sizeof(zfs_superblock_t));
        if (write_block(fs, 0, disk_buffer) != ZFS_OK) {
            return ZFS_ERROR;
        }
        
        fs->cache_dirty = 0;
    }
    
    // 标记为已卸载
    fs->mounted = 0;
    
    print_string("ZFS 文件系统卸载成功");
    print_newline();
    
    return ZFS_OK;
}

// 同步文件系统
int zfs_sync(zfs_fs_t* fs) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 与卸载类似，写回缓存
    if (fs->cache_dirty) {
        // 写回位图
        for (uint32_t i = 0; i < fs->superblock.bitmap_blocks && i < 16; i++) {
            if (write_block(fs, fs->superblock.bitmap_block + i,
                           fs->bitmap + i * ZFS_BLOCK_SIZE) != ZFS_OK) {
                return ZFS_ERROR;
            }
        }
        
        // 写回超级块
        memcpy(disk_buffer, &fs->superblock, sizeof(zfs_superblock_t));
        if (write_block(fs, 0, disk_buffer) != ZFS_OK) {
            return ZFS_ERROR;
        }
        
        fs->cache_dirty = 0;
    }
    
    return ZFS_OK;
}

// 根据路径找到inode
static int find_inode_by_path(zfs_fs_t* fs, const char* path, zfs_inode_t* inode) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 根目录特殊处理
    if (path[0] == '/' && path[1] == '\0') {
        return read_inode(fs, fs->superblock.root_inode, inode);
    }
    
    // 暂不处理复杂路径，简化为根目录下的文件
    if (path[0] == '/') {
        path++; // 跳过开头的'/'
    }
    
    // 读取根目录inode
    if (read_inode(fs, fs->superblock.root_inode, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 搜索目录项
    uint32_t dir_size = inode_cache.size;
    uint32_t entries_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_direntry_t);
    
    // 遍历目录的所有块
    for (uint32_t i = 0; i < 10 && i * ZFS_BLOCK_SIZE < dir_size; i++) {
        uint32_t block_num = inode_cache.direct_blocks[i];
        if (block_num == ZFS_INVALID_BLOCK) {
            continue;
        }
        
        // 读取目录块
        if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
            continue;
        }
        
        // 遍历目录项
        zfs_direntry_t* entries = (zfs_direntry_t*)disk_buffer;
        uint32_t count = (dir_size - i * ZFS_BLOCK_SIZE) / sizeof(zfs_direntry_t);
        if (count > entries_per_block) {
            count = entries_per_block;
        }
        
        for (uint32_t j = 0; j < count; j++) {
            if (entries[j].inode_num != ZFS_INVALID_BLOCK && 
                strcmp((char*)entries[j].filename, path) == 0) {
                // 找到匹配的文件
                return read_inode(fs, entries[j].inode_num, inode);
            }
        }
    }
    
    return ZFS_ERR_FILE_NOT_FOUND;
}

// 调试函数
void zfs_dump_info(zfs_fs_t* fs) {
    if (!fs) {
        print_string("ZFS: 无效的文件系统指针");
        print_newline();
        return;
    }
    
    print_string("ZFS 文件系统信息:");
    print_newline();
    
    print_string("  挂载状态: ");
    print_int(fs->mounted);
    print_newline();
    
    print_string("  魔数: 0x");
    print_int(fs->superblock.magic);
    print_newline();
    
    print_string("  版本: ");
    print_int(fs->superblock.version >> 8);
    print_string(".");
    print_int(fs->superblock.version & 0xFF);
    print_newline();
    
    print_string("  块大小: ");
    print_int(fs->superblock.block_size);
    print_string(" 字节");
    print_newline();
    
    print_string("  总块数: ");
    print_int(fs->superblock.total_blocks);
    print_newline();
    
    print_string("  数据块起始位置: ");
    print_int(fs->superblock.data_block);
    print_newline();
    
    print_string("  可用块数: ");
    print_int(fs->superblock.free_blocks);
    print_newline();
    
    print_string("  可用inode数: ");
    print_int(fs->superblock.free_inodes);
    print_newline();
    
    print_string("  卷标: ");
    for (int i = 0; i < 16 && fs->superblock.label[i]; i++) {
        print_char(fs->superblock.label[i]);
    }
    print_newline();
} 