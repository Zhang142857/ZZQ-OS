#include "zfs.h"
#include "string.h"

// 外部函数声明
extern void print_string(const char* str);
extern void print_int(int num);
extern void print_newline(void);
extern unsigned int get_tick(void);
extern zfs_fs_t* get_zfs_fs(void);

// 需要从zfs.c导入的内部函数，这些在实际实现中应该放在同一个文件
// 这里简单重新声明
static int read_block(zfs_fs_t* fs, uint32_t block_num, uint8_t* buffer);
static int write_block(zfs_fs_t* fs, uint32_t block_num, const uint8_t* buffer);
static int allocate_block(zfs_fs_t* fs);
static int free_block(zfs_fs_t* fs, uint32_t block_num);
static int read_inode(zfs_fs_t* fs, uint32_t inode_num, zfs_inode_t* inode);
static int write_inode(zfs_fs_t* fs, const zfs_inode_t* inode);
static int allocate_inode(zfs_fs_t* fs);
static int free_inode(zfs_fs_t* fs, uint32_t inode_num);
static int find_inode_by_path(zfs_fs_t* fs, const char* path, zfs_inode_t* inode);

// 缓冲区
static uint8_t disk_buffer[512];
static zfs_inode_t inode_cache;
static zfs_direntry_t dir_entries[512 / sizeof(zfs_direntry_t)];

// 创建文件或目录
int zfs_create(zfs_fs_t* fs, const char* path, uint8_t attributes) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 检查文件是否已存在
    if (find_inode_by_path(fs, path, &inode_cache) == ZFS_OK) {
        return ZFS_ERR_FILE_EXISTS;
    }
    
    // 提取文件名
    const char* filename = path;
    if (path[0] == '/') {
        filename = path + 1;
    }
    
    // 创建新inode
    int inode_num = allocate_inode(fs);
    if (inode_num < 0) {
        return inode_num; // 出错
    }
    
    // 初始化inode
    memset(&inode_cache, 0, sizeof(zfs_inode_t));
    inode_cache.inode_num = inode_num;
    strncpy((char*)inode_cache.filename, filename, ZFS_NAME_LENGTH - 1);
    inode_cache.attributes = attributes;
    inode_cache.size = 0;
    inode_cache.create_time = get_tick();
    inode_cache.modify_time = inode_cache.create_time;
    inode_cache.access_time = inode_cache.create_time;
    
    // 初始化块指针
    for (int i = 0; i < 10; i++) {
        inode_cache.direct_blocks[i] = ZFS_INVALID_BLOCK;
    }
    inode_cache.indirect_block = ZFS_INVALID_BLOCK;
    
    // 写入inode
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 读取根目录
    if (read_inode(fs, fs->superblock.root_inode, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 找到根目录的最后一个块，或分配一个新块
    uint32_t block_num = ZFS_INVALID_BLOCK;
    uint32_t block_index = inode_cache.size / ZFS_BLOCK_SIZE;
    uint32_t offset = inode_cache.size % ZFS_BLOCK_SIZE;
    
    if (block_index < 10) {
        if (inode_cache.direct_blocks[block_index] == ZFS_INVALID_BLOCK) {
            // 需要分配新块
            block_num = allocate_block(fs);
            if (block_num < 0) {
                return block_num; // 出错
            }
            inode_cache.direct_blocks[block_index] = block_num;
            
            // 初始化新块
            memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
        } else {
            // 使用现有块
            block_num = inode_cache.direct_blocks[block_index];
            
            // 读取现有块
            if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
                return ZFS_ERROR;
            }
        }
    } else {
        return ZFS_ERR_TOO_LARGE; // 目录过大
    }
    
    // 在块中添加目录项
    zfs_direntry_t* entry = (zfs_direntry_t*)(disk_buffer + offset);
    entry->inode_num = inode_num;
    strncpy((char*)entry->filename, filename, ZFS_NAME_LENGTH - 1);
    entry->attributes = attributes;
    
    // 更新根目录大小
    inode_cache.size += sizeof(zfs_direntry_t);
    inode_cache.modify_time = get_tick();
    
    // 写回块
    if (write_block(fs, block_num, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 写回根目录inode
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 删除文件或目录
int zfs_delete(zfs_fs_t* fs, const char* path) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 找到文件的inode
    if (find_inode_by_path(fs, path, &inode_cache) != ZFS_OK) {
        return ZFS_ERR_FILE_NOT_FOUND;
    }
    
    // 保存inode号备用
    uint32_t inode_num = inode_cache.inode_num;
    
    // 提取文件名
    const char* filename = path;
    if (path[0] == '/') {
        filename = path + 1;
    }
    
    // 读取根目录
    if (read_inode(fs, fs->superblock.root_inode, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 搜索目录项
    uint32_t dir_size = inode_cache.size;
    uint32_t entries_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_direntry_t);
    uint32_t entry_found = 0;
    uint32_t found_block = 0;
    uint32_t found_offset = 0;
    
    // 遍历目录的所有块
    for (uint32_t i = 0; i < 10 && i * ZFS_BLOCK_SIZE < dir_size && !entry_found; i++) {
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
                strcmp((char*)entries[j].filename, filename) == 0) {
                // 找到匹配的文件
                entry_found = 1;
                found_block = block_num;
                found_offset = j * sizeof(zfs_direntry_t);
                break;
            }
        }
    }
    
    if (!entry_found) {
        return ZFS_ERR_FILE_NOT_FOUND;
    }
    
    // 读取包含目录项的块
    if (read_block(fs, found_block, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 标记目录项为删除
    zfs_direntry_t* entry = (zfs_direntry_t*)(disk_buffer + found_offset);
    entry->inode_num = ZFS_INVALID_BLOCK;
    
    // 写回块
    if (write_block(fs, found_block, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 释放inode及其关联的块
    if (free_inode(fs, inode_num) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 打开文件
int zfs_open(zfs_fs_t* fs, const char* path, uint8_t mode, zfs_file_t* file) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 找到文件的inode
    if (find_inode_by_path(fs, path, &inode_cache) != ZFS_OK) {
        return ZFS_ERR_FILE_NOT_FOUND;
    }
    
    // 检查权限
    if ((mode & 0x02) && (inode_cache.attributes & ZFS_ATTR_READONLY)) {
        return ZFS_ERR_READONLY;
    }
    
    // 初始化文件描述符
    file->inode_num = inode_cache.inode_num;
    file->position = 0;
    file->mode = mode;
    
    // 更新访问时间
    inode_cache.access_time = get_tick();
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 关闭文件
int zfs_close(zfs_fs_t* fs, zfs_file_t* file) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 没有特别需要做的，返回成功
    return ZFS_OK;
}

// 读取文件
int zfs_read(zfs_fs_t* fs, zfs_file_t* file, void* buffer, uint32_t size) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 读取文件inode
    if (read_inode(fs, file->inode_num, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 检查是否超出文件大小
    if (file->position >= inode_cache.size) {
        return 0; // 已经到达文件末尾
    }
    
    // 调整大小
    if (file->position + size > inode_cache.size) {
        size = inode_cache.size - file->position;
    }
    
    // 读取数据
    uint32_t bytes_read = 0;
    uint8_t* buf = (uint8_t*)buffer;
    
    while (bytes_read < size) {
        uint32_t block_index = file->position / ZFS_BLOCK_SIZE;
        uint32_t offset = file->position % ZFS_BLOCK_SIZE;
        uint32_t bytes_to_read = ZFS_BLOCK_SIZE - offset;
        
        if (bytes_to_read > size - bytes_read) {
            bytes_to_read = size - bytes_read;
        }
        
        if (block_index < 10) {
            uint32_t block_num = inode_cache.direct_blocks[block_index];
            if (block_num == ZFS_INVALID_BLOCK) {
                break; // 块不存在，读取结束
            }
            
            // 读取块
            if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
                return ZFS_ERROR;
            }
            
            // 复制数据
            memcpy(buf + bytes_read, disk_buffer + offset, bytes_to_read);
        } else {
            // 间接块暂不实现
            break;
        }
        
        bytes_read += bytes_to_read;
        file->position += bytes_to_read;
    }
    
    // 更新访问时间
    inode_cache.access_time = get_tick();
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return bytes_read;
}

// 写入文件
int zfs_write(zfs_fs_t* fs, zfs_file_t* file, const void* buffer, uint32_t size) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 读取文件inode
    if (read_inode(fs, file->inode_num, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 检查写权限
    if (inode_cache.attributes & ZFS_ATTR_READONLY) {
        return ZFS_ERR_READONLY;
    }
    
    // 检查文件大小上限
    if (file->position + size > ZFS_MAX_FILE_SIZE) {
        return ZFS_ERR_TOO_LARGE;
    }
    
    // 写入数据
    uint32_t bytes_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;
    
    while (bytes_written < size) {
        uint32_t block_index = file->position / ZFS_BLOCK_SIZE;
        uint32_t offset = file->position % ZFS_BLOCK_SIZE;
        uint32_t bytes_to_write = ZFS_BLOCK_SIZE - offset;
        
        if (bytes_to_write > size - bytes_written) {
            bytes_to_write = size - bytes_written;
        }
        
        if (block_index < 10) {
            uint32_t block_num = inode_cache.direct_blocks[block_index];
            if (block_num == ZFS_INVALID_BLOCK) {
                // 需要分配新块
                block_num = allocate_block(fs);
                if (block_num < 0) {
                    return block_num; // 出错
                }
                inode_cache.direct_blocks[block_index] = block_num;
                
                // 初始化新块
                memset(disk_buffer, 0, ZFS_BLOCK_SIZE);
            } else {
                // 读取现有块
                if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
                    return ZFS_ERROR;
                }
            }
            
            // 写入数据
            memcpy(disk_buffer + offset, buf + bytes_written, bytes_to_write);
            
            // 写回块
            if (write_block(fs, block_num, disk_buffer) != ZFS_OK) {
                return ZFS_ERROR;
            }
        } else {
            // 间接块暂不实现
            return ZFS_ERR_TOO_LARGE;
        }
        
        bytes_written += bytes_to_write;
        file->position += bytes_to_write;
    }
    
    // 更新文件大小
    if (file->position > inode_cache.size) {
        inode_cache.size = file->position;
    }
    
    // 更新修改时间
    inode_cache.modify_time = get_tick();
    
    // 写回inode
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return bytes_written;
}

// 移动文件指针
int zfs_seek(zfs_fs_t* fs, zfs_file_t* file, uint32_t offset) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 读取文件inode
    if (read_inode(fs, file->inode_num, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 检查偏移量
    if (offset > inode_cache.size) {
        offset = inode_cache.size;
    }
    
    // 设置新位置
    file->position = offset;
    
    return ZFS_OK;
}

// 获取文件信息
int zfs_stat(zfs_fs_t* fs, const char* path, zfs_inode_t* inode) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 查找文件inode
    int result = find_inode_by_path(fs, path, &inode_cache);
    if (result != ZFS_OK) {
        return result;
    }
    
    // 复制inode信息
    memcpy(inode, &inode_cache, sizeof(zfs_inode_t));
    
    return ZFS_OK;
}

// 列出目录内容
int zfs_list_directory(zfs_fs_t* fs, const char* path, zfs_direntry_t* entries, uint32_t* count) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 找到目录inode
    int result = find_inode_by_path(fs, path, &inode_cache);
    if (result != ZFS_OK) {
        return result;
    }
    
    // 检查是否为目录
    if (!(inode_cache.attributes & ZFS_ATTR_DIRECTORY)) {
        return ZFS_ERROR; // 不是目录
    }
    
    // 读取目录内容
    uint32_t dir_size = inode_cache.size;
    uint32_t entries_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_direntry_t);
    uint32_t entry_count = 0;
    
    // 遍历目录的所有块
    for (uint32_t i = 0; i < 10 && i * ZFS_BLOCK_SIZE < dir_size && entry_count < *count; i++) {
        uint32_t block_num = inode_cache.direct_blocks[i];
        if (block_num == ZFS_INVALID_BLOCK) {
            continue;
        }
        
        // 读取目录块
        if (read_block(fs, block_num, disk_buffer) != ZFS_OK) {
            continue;
        }
        
        // 遍历目录项
        zfs_direntry_t* dir_entries = (zfs_direntry_t*)disk_buffer;
        uint32_t block_entries = (dir_size - i * ZFS_BLOCK_SIZE) / sizeof(zfs_direntry_t);
        if (block_entries > entries_per_block) {
            block_entries = entries_per_block;
        }
        
        for (uint32_t j = 0; j < block_entries && entry_count < *count; j++) {
            if (dir_entries[j].inode_num != ZFS_INVALID_BLOCK) {
                // 复制有效目录项
                memcpy(&entries[entry_count], &dir_entries[j], sizeof(zfs_direntry_t));
                entry_count++;
            }
        }
    }
    
    // 更新实际条目数
    *count = entry_count;
    
    // 更新访问时间
    inode_cache.access_time = get_tick();
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
}

// 重命名文件或目录
int zfs_rename(zfs_fs_t* fs, const char* old_path, const char* new_path) {
    if (!fs->mounted) {
        return ZFS_ERR_NOT_MOUNTED;
    }
    
    // 提取文件名
    const char* old_filename = old_path;
    if (old_path[0] == '/') {
        old_filename = old_path + 1;
    }
    
    const char* new_filename = new_path;
    if (new_path[0] == '/') {
        new_filename = new_path + 1;
    }
    
    // 检查新文件名是否已存在
    if (find_inode_by_path(fs, new_path, &inode_cache) == ZFS_OK) {
        return ZFS_ERR_FILE_EXISTS;
    }
    
    // 读取根目录
    if (read_inode(fs, fs->superblock.root_inode, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 搜索旧文件名的目录项
    uint32_t dir_size = inode_cache.size;
    uint32_t entries_per_block = ZFS_BLOCK_SIZE / sizeof(zfs_direntry_t);
    uint32_t entry_found = 0;
    uint32_t found_block = 0;
    uint32_t found_offset = 0;
    
    // 遍历目录的所有块
    for (uint32_t i = 0; i < 10 && i * ZFS_BLOCK_SIZE < dir_size && !entry_found; i++) {
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
                strcmp((char*)entries[j].filename, old_filename) == 0) {
                // 找到匹配的文件
                entry_found = 1;
                found_block = block_num;
                found_offset = j * sizeof(zfs_direntry_t);
                break;
            }
        }
    }
    
    if (!entry_found) {
        return ZFS_ERR_FILE_NOT_FOUND;
    }
    
    // 读取包含目录项的块
    if (read_block(fs, found_block, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 更新文件名
    zfs_direntry_t* entry = (zfs_direntry_t*)(disk_buffer + found_offset);
    strncpy((char*)entry->filename, new_filename, ZFS_NAME_LENGTH - 1);
    
    // 写回块
    if (write_block(fs, found_block, disk_buffer) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    // 读取文件inode并更新文件名
    if (read_inode(fs, entry->inode_num, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    strncpy((char*)inode_cache.filename, new_filename, ZFS_NAME_LENGTH - 1);
    inode_cache.modify_time = get_tick();
    
    // 写回inode
    if (write_inode(fs, &inode_cache) != ZFS_OK) {
        return ZFS_ERROR;
    }
    
    return ZFS_OK;
} 