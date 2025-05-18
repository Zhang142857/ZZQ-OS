#include "fs.h"
#include "memory.h"
#include "string.h"

// 外部函数声明
extern void print_string(const char* str);
extern void print_newline(void);
extern void int_to_string(int num, char* str);
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// 前向声明，解决隐式声明问题
void disk_use_data_storage();
void disk_use_system_storage();

// 文件系统状态
static int fs_initialized = 0;
static int fs_mounted = 0;
static int fs_persistent_enabled = 0; // 新增：标记是否启用持久化功能
static filesystem_t fs;

// 16位端口输入函数
unsigned short inw(unsigned short port) {
    unsigned short result;
    __asm__("in %%dx, %%ax" : "=a" (result) : "d" (port));
    return result;
}

// 16位端口输出函数
void outw(unsigned short port, unsigned short data) {
    __asm__("out %%ax, %%dx" : : "a" (data), "d" (port));
}

// ATA磁盘端口定义 - 使用第二个磁盘 (Secondary ATA)
#define ATA_DATA            0x170
#define ATA_ERROR           0x171
#define ATA_SECTOR_COUNT    0x172
#define ATA_LBA_LOW         0x173
#define ATA_LBA_MID         0x174
#define ATA_LBA_HIGH        0x175
#define ATA_DRIVE_HEAD      0x176
#define ATA_STATUS          0x177
#define ATA_COMMAND         0x177
#define ATA_CONTROL         0x376

// ATA命令
#define ATA_CMD_READ_SECTORS     0x20
#define ATA_CMD_WRITE_SECTORS    0x30

// ATA状态位
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80

// 内存磁盘模拟 - 用于系统盘
#define DISK_SECTORS 1024
static char disk_image[DISK_SECTORS * SECTOR_SIZE] = {0};

// 磁盘缓存 - 用于数据盘
#define CACHE_SECTORS 32
static char sector_cache[CACHE_SECTORS][SECTOR_SIZE] = {0};
static unsigned int cached_sectors[CACHE_SECTORS] = {0};
static int cache_valid[CACHE_SECTORS] = {0};
static int cache_dirty[CACHE_SECTORS] = {0};
static int next_cache_slot = 0;

// 文件系统布局常量
#define FS_SUPERBLOCK_SECTOR 1
#define FS_FILE_TABLE_SECTOR 2
#define FS_DATA_START_SECTOR 8

// 文件系统状态访问函数
void fs_set_initialized(int value) {
    fs_initialized = value;
}

void fs_set_mounted(int value) {
    fs_mounted = value;
}

int fs_is_initialized() {
    return fs_initialized;
}

int fs_is_mounted() {
    return fs_mounted;
}

// 启用持久化存储 - 由kernel.c在系统稳定后调用
void fs_enable_persistence() {
    fs_persistent_enabled = 1;
    print_string("Data persistence enabled.");
    print_newline();
}

// ATA磁盘等待就绪
void ata_wait_ready() {
    // 如果未启用持久化，直接返回
    if (!fs_persistent_enabled) return;
    
    // 等待BSY位清零，但设置超时，防止死循环
    int timeout = 10000; // 安全超时
    while ((inb(ATA_STATUS) & ATA_STATUS_BSY) && timeout > 0) {
        timeout--;
    }
}

// ATA磁盘读取扇区 - 从第二个磁盘读取
int ata_read_sector(unsigned int lba, void* buffer) {
    // 如果未启用持久化，直接返回失败
    if (!fs_persistent_enabled) return 0;
    
    // 等待磁盘就绪
    ata_wait_ready();
    if (!fs_persistent_enabled) return 0; // 双重检查
    
    // 设置要读取的扇区数（1个）
    outb(ATA_SECTOR_COUNT, 1);
    
    // 设置LBA地址
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // 设置驱动器和LBA模式（第2个磁盘，LBA模式）
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F));
    
    // 发送读扇区命令
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);
    
    // 等待数据就绪
    ata_wait_ready();
    if (inb(ATA_STATUS) & ATA_STATUS_ERR) {
        return 0;
    }
    
    // 读取数据（每次读取一个字）
    unsigned short* buf = (unsigned short*)buffer;
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        buf[i] = inw(ATA_DATA);
    }
    
    return 1;
}

// ATA磁盘写入扇区 - 写入第二个磁盘
int ata_write_sector(unsigned int lba, const void* buffer) {
    // 如果未启用持久化，直接返回失败
    if (!fs_persistent_enabled) return 0;
    
    // 等待磁盘就绪
    ata_wait_ready();
    if (!fs_persistent_enabled) return 0; // 双重检查
    
    // 设置要写入的扇区数（1个）
    outb(ATA_SECTOR_COUNT, 1);
    
    // 设置LBA地址
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // 设置驱动器和LBA模式（第2个磁盘，LBA模式）
    outb(ATA_DRIVE_HEAD, 0xF0 | ((lba >> 24) & 0x0F));
    
    // 发送写扇区命令
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    // 等待磁盘准备接收数据
    ata_wait_ready();
    if (inb(ATA_STATUS) & ATA_STATUS_ERR) {
        return 0;
    }
    
    // 写入数据（每次写入一个字）
    unsigned short* buf = (unsigned short*)buffer;
    for (int i = 0; i < SECTOR_SIZE / 2; i++) {
        outw(ATA_DATA, buf[i]);
    }
    
    // 等待写入完成
    ata_wait_ready();
    if (inb(ATA_STATUS) & ATA_STATUS_ERR) {
        return 0;
    }
    
    return 1;
}

// 从缓存中查找扇区
int find_in_cache(unsigned int sector) {
    for (int i = 0; i < CACHE_SECTORS; i++) {
        if (cache_valid[i] && cached_sectors[i] == sector) {
            return i;
        }
    }
    return -1;
}

// 磁盘读写操作 - 所有操作首先使用内存，持久化启用后才使用ATA
int disk_read_sector(unsigned int sector, void* buffer) {
    if (sector >= DISK_SECTORS) {
        return 0;
    }
    
    // 始终从内存读取
    unsigned int offset = sector * SECTOR_SIZE;
    memcpy(buffer, &disk_image[offset], SECTOR_SIZE);
    
    // 如果持久化未启用，直接返回内存中的数据
    if (!fs_persistent_enabled) {
        return 1;
    }
    
    // 持久化已启用，检查缓存
    int cache_idx = find_in_cache(sector);
    if (cache_idx >= 0) {
        // 从缓存中读取
        memcpy(buffer, sector_cache[cache_idx], SECTOR_SIZE);
        return 1;
    }
    
    // 不在缓存中，尝试从ATA读取
    char ata_buffer[SECTOR_SIZE];
    if (ata_read_sector(sector, ata_buffer)) {
        // 添加到缓存
        cache_idx = next_cache_slot;
        
        // 如果当前缓存位置有脏数据，先写回磁盘
        if (cache_valid[cache_idx] && cache_dirty[cache_idx]) {
            ata_write_sector(cached_sectors[cache_idx], sector_cache[cache_idx]);
        }
        
        // 更新缓存和内存镜像
        memcpy(sector_cache[cache_idx], ata_buffer, SECTOR_SIZE);
        memcpy(&disk_image[offset], ata_buffer, SECTOR_SIZE);
        memcpy(buffer, ata_buffer, SECTOR_SIZE);
        
        cached_sectors[cache_idx] = sector;
        cache_valid[cache_idx] = 1;
        cache_dirty[cache_idx] = 0;
        
        // 更新下一个缓存位置
        next_cache_slot = (next_cache_slot + 1) % CACHE_SECTORS;
    }
    
    return 1;
}

int disk_write_sector(unsigned int sector, const void* buffer) {
    if (sector >= DISK_SECTORS) {
        return 0;
    }
    
    // 始终写入内存
    unsigned int offset = sector * SECTOR_SIZE;
    memcpy(&disk_image[offset], buffer, SECTOR_SIZE);
    
    // 如果持久化未启用，到此为止
    if (!fs_persistent_enabled) {
        return 1;
    }
    
    // 持久化已启用，处理缓存
    int cache_idx = find_in_cache(sector);
    if (cache_idx < 0) {
        // 不在缓存中，使用下一个可用的缓存位置
        cache_idx = next_cache_slot;
        
        // 如果当前缓存位置有脏数据，先写回磁盘
        if (cache_valid[cache_idx] && cache_dirty[cache_idx]) {
            ata_write_sector(cached_sectors[cache_idx], sector_cache[cache_idx]);
        }
        
        // 更新缓存信息
        cached_sectors[cache_idx] = sector;
        cache_valid[cache_idx] = 1;
    }
    
    // 更新缓存内容并标记为脏
    memcpy(sector_cache[cache_idx], buffer, SECTOR_SIZE);
    cache_dirty[cache_idx] = 1;
    
    // 更新下一个缓存位置
    if (cache_idx == next_cache_slot) {
        next_cache_slot = (next_cache_slot + 1) % CACHE_SECTORS;
    }
    
    return 1;
}

// 将缓存刷新到磁盘
int fs_disk_flush() {
    // 如果持久化未启用，直接返回成功
    if (!fs_persistent_enabled) {
        return 1;
    }
    
    // 将所有脏缓存写回磁盘
    for (int i = 0; i < CACHE_SECTORS; i++) {
        if (cache_valid[i] && cache_dirty[i]) {
            if (!ata_write_sector(cached_sectors[i], sector_cache[i])) {
                // 写入失败，简单记录错误
                print_string("Warning: Failed to flush sector cache!");
                print_newline();
            } else {
                cache_dirty[i] = 0;
            }
        }
    }
    return 1;
}

// 查找空闲文件条目
int find_free_file_entry() {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs.file_table[i].is_used) {
            return i;
        }
    }
    return -1;
}

// 根据文件名查找文件条目
int find_file_entry(const char* filename) {
    if (!filename || !filename[0]) {
        return -1;
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.file_table[i].is_used && 
            strcmp(fs.file_table[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

// 保存文件系统元数据到磁盘
int save_fs_metadata() {
    // 安全检查
    if (!fs_is_initialized()) {
        return 0;
    }
    
    // 保存超级块
    char buffer[SECTOR_SIZE];
    memset(buffer, 0, SECTOR_SIZE);
    
    // 确保数据字段有效
    if (fs.num_sectors > DISK_SECTORS) fs.num_sectors = DISK_SECTORS;
    if (fs.free_sector >= DISK_SECTORS) fs.free_sector = FS_DATA_START_SECTOR;
    
    // 复制文件系统元数据
    memcpy(buffer, &fs, sizeof(filesystem_t) < SECTOR_SIZE ? sizeof(filesystem_t) : SECTOR_SIZE);
    
    // 写入超级块
    if (!disk_write_sector(FS_SUPERBLOCK_SECTOR, buffer)) {
        return 0;
    }
    
    // 保存文件表 - 使用多个扇区
    int entries_per_sector = SECTOR_SIZE / sizeof(file_entry_t);
    if (entries_per_sector == 0) entries_per_sector = 1; // 避免除零错误
    
    int sectors_needed = (MAX_FILES + entries_per_sector - 1) / entries_per_sector;
    if (sectors_needed > 5) sectors_needed = 5; // 限制最大扇区数，避免过多写入
    
    for (int sector = 0; sector < sectors_needed; sector++) {
        memset(buffer, 0, SECTOR_SIZE);
        
        int start_entry = sector * entries_per_sector;
        if (start_entry >= MAX_FILES) break;
        
        int end_entry = start_entry + entries_per_sector;
        if (end_entry > MAX_FILES) end_entry = MAX_FILES;
        
        // 计算要复制的字节数
        int bytes_to_copy = (end_entry - start_entry) * sizeof(file_entry_t);
        if (bytes_to_copy > SECTOR_SIZE) bytes_to_copy = SECTOR_SIZE;
        
        // 复制文件表数据
        memcpy(buffer, &fs.file_table[start_entry], bytes_to_copy);
        
        // 写入扇区
        if (!disk_write_sector(FS_FILE_TABLE_SECTOR + sector, buffer)) {
            return 0;
        }
    }
    
    return 1;
}

// 初始化文件系统
void fs_init() {
    // 初始化文件系统结构
    fs.num_sectors = DISK_SECTORS;
    fs.data_start = FS_DATA_START_SECTOR;
    fs.free_sector = FS_DATA_START_SECTOR;
    
    // 清空文件表
    for (int i = 0; i < MAX_FILES; i++) {
        memset(&fs.file_table[i], 0, sizeof(file_entry_t));
        fs.file_table[i].is_used = 0;
    }
    
    // 设置状态
    fs_set_initialized(1);
    fs_set_mounted(0);
    
    // 切换到数据盘模式
    disk_use_data_storage();
}

// 格式化文件系统
int fs_format() {
    // 确保文件系统已初始化
    if (!fs_is_initialized()) {
        fs_init();
    }
    
    // 初始化文件系统结构
    fs.num_sectors = DISK_SECTORS;
    fs.data_start = FS_DATA_START_SECTOR;
    fs.free_sector = FS_DATA_START_SECTOR;
    
    // 清空文件表
    for (int i = 0; i < MAX_FILES; i++) {
        memset(&fs.file_table[i], 0, sizeof(file_entry_t));
        fs.file_table[i].is_used = 0;
    }
    
    // 保存文件系统元数据
    if (!save_fs_metadata()) {
        print_string("Failed to save filesystem metadata!");
        print_newline();
        return 0;
    }
    
    // 确保数据刷新到磁盘
    if (fs_persistent_enabled) {
        fs_disk_flush();
    }
    
    // 设置状态为已挂载
    fs_set_mounted(1);
    
    return 1;
}

// 挂载文件系统
int fs_mount() {
    // 确保文件系统已初始化
    if (!fs_is_initialized()) {
        fs_init();
    }
    
    // 读取超级块
    char buffer[SECTOR_SIZE];
    if (!disk_read_sector(FS_SUPERBLOCK_SECTOR, buffer)) {
        print_string("Failed to read superblock!");
        print_newline();
        return 0;
    }
    
    // 复制超级块数据
    memcpy(&fs, buffer, sizeof(filesystem_t));
    
    // 读取文件表
    int entries_per_sector = SECTOR_SIZE / sizeof(file_entry_t);
    int sectors_needed = (MAX_FILES + entries_per_sector - 1) / entries_per_sector;
    
    for (int sector = 0; sector < sectors_needed; sector++) {
        if (!disk_read_sector(FS_FILE_TABLE_SECTOR + sector, buffer)) {
            print_string("Failed to read file table!");
            print_newline();
            return 0;
        }
        
        int start_entry = sector * entries_per_sector;
        int end_entry = start_entry + entries_per_sector;
        if (end_entry > MAX_FILES) end_entry = MAX_FILES;
        
        int bytes_to_copy = (end_entry - start_entry) * sizeof(file_entry_t);
        memcpy(&fs.file_table[start_entry], buffer, bytes_to_copy);
    }
    
    // 设置状态
    fs_set_mounted(1);
    
    // 确保数据缓存与磁盘同步
    if (fs_persistent_enabled) {
        fs_disk_flush();
    }
    
    return 1;
}

// 打开文件
file_t fs_open(const char* filename, int create) {
    file_t file;
    file.file_index = -1;
    file.position = 0;
    
    // 安全检查 - 文件系统是否已挂载
    if (!fs_is_mounted()) {
        return file;
    }
    
    // 安全检查 - 文件名是否有效
    if (!filename || !filename[0] || strlen(filename) >= MAX_FILENAME_LENGTH) {
        return file;
    }
    
    // 查找文件
    int file_index = find_file_entry(filename);
    
    // 如果文件不存在且需要创建
    if (file_index == -1 && create) {
        // 查找空闲文件条目
        file_index = find_free_file_entry();
        if (file_index == -1) {
            return file;
        }
        
        // 避免越界访问
        if (file_index < 0 || file_index >= MAX_FILES) {
            return file;
        }
        
        // 安全检查 - 下一个空闲扇区是否有效
        if (fs.free_sector >= DISK_SECTORS) {
            return file;
        }
        
        // 初始化新文件条目
        memset(&fs.file_table[file_index], 0, sizeof(file_entry_t));
        
        // 安全复制文件名 (自己实现strncpy)
        int i;
        for (i = 0; i < MAX_FILENAME_LENGTH - 1 && filename[i] != '\0'; i++) {
            fs.file_table[file_index].filename[i] = filename[i];
        }
        fs.file_table[file_index].filename[i] = '\0';
        
        fs.file_table[file_index].size = 0;
        fs.file_table[file_index].attributes = FILE_ATTR_NORMAL;
        fs.file_table[file_index].is_used = 1;
        fs.file_table[file_index].start_sector = fs.free_sector++;
        
        // 保存文件系统元数据
        save_fs_metadata();
    } else if (file_index == -1) {
        // 文件不存在且不需要创建
        return file;
    }
    
    // 安全检查 - 文件索引是否有效
    if (file_index < 0 || file_index >= MAX_FILES) {
        return file;
    }
    
    // 设置文件句柄
    file.file_index = file_index;
    file.position = 0;
    
    return file;
}

// 关闭文件
int fs_close(file_t* file) {
    if (!file || file->file_index == -1) {
        return 0;
    }
    
    // 重置文件句柄
    file->file_index = -1;
    file->position = 0;
    
    return 1;
}

// 读取文件
int fs_read(file_t* file, void* buffer, unsigned int size) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    if (!file || file->file_index == -1 || !buffer || size == 0) {
        return 0;
    }
    
    // 获取文件条目
    file_entry_t* entry = &fs.file_table[file->file_index];
    
    // 检查是否达到文件末尾
    if (file->position >= entry->size) {
        return 0;
    }
    
    // 计算实际读取的字节数
    unsigned int bytes_to_read = size;
    if (file->position + bytes_to_read > entry->size) {
        bytes_to_read = entry->size - file->position;
    }
    
    // 计算起始扇区和偏移
    unsigned int start_sector = entry->start_sector + (file->position / SECTOR_SIZE);
    unsigned int offset = file->position % SECTOR_SIZE;
    
    // 逐扇区读取
    unsigned int bytes_read = 0;
    char sector_buffer[SECTOR_SIZE];
    
    while (bytes_read < bytes_to_read) {
        // 读取当前扇区
        if (!disk_read_sector(start_sector, sector_buffer)) {
            return bytes_read;
        }
        
        // 计算本次读取的字节数
        unsigned int bytes_this_sector = SECTOR_SIZE - offset;
        if (bytes_read + bytes_this_sector > bytes_to_read) {
            bytes_this_sector = bytes_to_read - bytes_read;
        }
        
        // 复制数据
        memcpy((char*)buffer + bytes_read, sector_buffer + offset, bytes_this_sector);
        
        // 更新已读字节数和位置
        bytes_read += bytes_this_sector;
        file->position += bytes_this_sector;
        
        // 为下一个扇区准备
        start_sector++;
        offset = 0;
    }
    
    return bytes_read;
}

// 写入文件
int fs_write(file_t* file, const void* buffer, unsigned int size) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    if (!file || file->file_index == -1 || !buffer || size == 0) {
        return 0;
    }
    
    // 获取文件条目
    file_entry_t* entry = &fs.file_table[file->file_index];
    
    // 检查文件大小限制
    if (file->position + size > MAX_FILE_SIZE) {
        print_string("Error: File size limit exceeded!");
        print_newline();
        return 0;
    }
    
    // 计算起始扇区和偏移
    unsigned int start_sector = entry->start_sector + (file->position / SECTOR_SIZE);
    unsigned int offset = file->position % SECTOR_SIZE;
    
    // 逐扇区写入
    unsigned int bytes_written = 0;
    char sector_buffer[SECTOR_SIZE];
    
    while (bytes_written < size) {
        // 读取当前扇区（可能需要部分更新）
        memset(sector_buffer, 0, SECTOR_SIZE);
        if (offset > 0 || (bytes_written + SECTOR_SIZE - offset) > size) {
            if (!disk_read_sector(start_sector, sector_buffer)) {
                return bytes_written;
            }
        }
        
        // 计算本次写入的字节数
        unsigned int bytes_this_sector = SECTOR_SIZE - offset;
        if (bytes_written + bytes_this_sector > size) {
            bytes_this_sector = size - bytes_written;
        }
        
        // 复制数据
        memcpy(sector_buffer + offset, (const char*)buffer + bytes_written, bytes_this_sector);
        
        // 写入扇区
        if (!disk_write_sector(start_sector, sector_buffer)) {
            return bytes_written;
        }
        
        // 更新已写字节数和位置
        bytes_written += bytes_this_sector;
        file->position += bytes_this_sector;
        
        // 为下一个扇区准备
        start_sector++;
        offset = 0;
    }
    
    // 更新文件大小
    if (file->position > entry->size) {
        entry->size = file->position;
        save_fs_metadata();
    }
    
    // 添加：确保数据刷新到磁盘
    if (fs_persistent_enabled) {
        fs_disk_flush();
    }
    
    return bytes_written;
}

// 删除文件
int fs_delete(const char* filename) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    // 查找文件
    int file_index = find_file_entry(filename);
    if (file_index == -1) {
        return 0;
    }
    
    // 标记文件条目为未使用
    fs.file_table[file_index].is_used = 0;
    
    // 保存文件系统元数据
    if (!save_fs_metadata()) {
        return 0;
    }
    
    // 确保数据刷新到磁盘
    if (fs_persistent_enabled) {
        fs_disk_flush();
    }
    
    return 1;
}

// 重命名文件
int fs_rename(const char* oldname, const char* newname) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    // 查找旧文件
    int file_index = find_file_entry(oldname);
    if (file_index == -1) {
        return 0;
    }
    
    // 检查新文件名是否已存在
    if (find_file_entry(newname) != -1) {
        return 0;
    }
    
    // 更新文件名
    strcpy(fs.file_table[file_index].filename, newname);
    
    // 保存文件系统元数据
    if (!save_fs_metadata()) {
        return 0;
    }
    
    // 确保数据刷新到磁盘
    if (fs_persistent_enabled) {
        fs_disk_flush();
    }
    
    return 1;
}

// 调整文件指针位置
int fs_seek(file_t* file, unsigned int position) {
    if (!file || file->file_index == -1) {
        return 0;
    }
    
    // 获取文件条目
    file_entry_t* entry = &fs.file_table[file->file_index];
    
    // 检查位置是否有效
    if (position > entry->size) {
        return 0;
    }
    
    // 更新位置
    file->position = position;
    
    return 1;
}

// 列出所有文件
int fs_list_files(char* buffer, unsigned int buffer_size) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    if (!buffer || buffer_size == 0) {
        return 0;
    }
    
    // 清空缓冲区
    buffer[0] = '\0';
    
    // 记录文件数量
    int file_count = 0;
    
    // 收集文件列表
    unsigned int pos = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs.file_table[i].is_used) {
            file_count++;
            
            // 检查缓冲区空间
            unsigned int name_len = strlen(fs.file_table[i].filename);
            if (pos + name_len + 16 > buffer_size) {
                break;
            }
            
            // 添加文件名
            strcpy(buffer + pos, fs.file_table[i].filename);
            pos += name_len;
            
            // 添加文件大小
            strcpy(buffer + pos, " (");
            pos += 2;
            
            // 转换文件大小为字符串
            char size_str[16];
            int size_val = fs.file_table[i].size;
            int idx = 0;
            
            // 处理0的特殊情况
            if (size_val == 0) {
                size_str[idx++] = '0';
            } else {
                // 转换数字为字符串
                while (size_val > 0) {
                    size_str[idx++] = '0' + (size_val % 10);
                    size_val /= 10;
                }
            }
            
            // 反转数字字符串
            for (int j = 0; j < idx / 2; j++) {
                char temp = size_str[j];
                size_str[j] = size_str[idx - j - 1];
                size_str[idx - j - 1] = temp;
            }
            
            // 添加单位和结束括号
            size_str[idx++] = 'B';
            size_str[idx++] = ')';
            size_str[idx] = '\0';
            
            // 复制到缓冲区
            strcpy(buffer + pos, size_str);
            pos += idx;
            
            // 添加换行符
            buffer[pos++] = '\n';
            buffer[pos] = '\0';
        }
    }
    
    // 如果没有文件，显示提示信息
    if (file_count == 0) {
        strcpy(buffer, "No files found.\n");
    }
    
    return 1;
}

// 获取文件大小
unsigned int fs_get_file_size(const char* filename) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    // 查找文件
    int file_index = find_file_entry(filename);
    if (file_index == -1) {
        return 0;
    }
    
    return fs.file_table[file_index].size;
}

// 检查文件是否存在
int fs_file_exists(const char* filename) {
    if (!fs_is_mounted()) {
        return 0;
    }
    
    return (find_file_entry(filename) != -1);
}

// 切换访问目标到数据盘
void disk_use_data_storage() {
    static int is_data_disk = 0;
    is_data_disk = 1;
}

// 切换访问目标到系统盘（内存模拟）
void disk_use_system_storage() {
    static int is_data_disk = 0;
    is_data_disk = 0;
} 