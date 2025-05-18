#include "fat32.h"
#include "string.h"
#include "memory.h"

// 外部函数声明
extern void print_string(const char* str);
extern void print_newline(void);
extern void int_to_string(int num, char* str);

// 打开的文件列表
static fat32_file_t fat32_open_files[FAT32_MAX_OPEN_FILES];
static uint8_t fat32_sector_buffer[512]; // 共享的扇区缓冲区

// 辅助函数: 填充一个扇区大小的缓冲区
static void fill_sector(uint8_t* buffer, uint8_t value) {
    for (int i = 0; i < 512; i++) {
        buffer[i] = value;
    }
}

// 辅助函数: 获取FAT表中指定簇的下一个簇号
static uint32_t fat32_get_next_cluster(fat32_t* fs, uint32_t cluster) {
    if (cluster < 2) return 0;
    
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    // 读取FAT扇区
    uint8_t buffer[512];
    if (!disk_read(fs->disk, fs->start_lba + fat_sector, 1, buffer)) {
        return 0;
    }
    
    // 获取FAT表项
    uint32_t next_cluster = *((uint32_t*)&buffer[entry_offset]);
    next_cluster &= 0x0FFFFFFF; // FAT32项是28位
    
    return next_cluster;
}

// 辅助函数: 设置FAT表中指定簇的值
static int fat32_set_next_cluster(fat32_t* fs, uint32_t cluster, uint32_t next_cluster) {
    if (cluster < 2) return 0;
    
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    // 读取FAT扇区
    uint8_t buffer[512];
    if (!disk_read(fs->disk, fs->start_lba + fat_sector, 1, buffer)) {
        return 0;
    }
    
    // 设置FAT表项，保留高4位
    uint32_t* fat_entry = (uint32_t*)&buffer[entry_offset];
    *fat_entry = (*fat_entry & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
    
    // 写回FAT表扇区
    if (!disk_write(fs->disk, fs->start_lba + fat_sector, 1, buffer)) {
        return 0;
    }
    
    // FAT32文件系统通常有两个相同的FAT表，也需要更新第二个表
    if (fs->bpb.num_fats > 1) {
        uint32_t fat2_sector = fat_sector + fs->bpb.fat_size_32;
        
        if (!disk_write(fs->disk, fs->start_lba + fat2_sector, 1, buffer)) {
            return 0;
        }
    }
    
    return 1;
}

// 辅助函数: 分配一个新簇
static uint32_t fat32_allocate_cluster(fat32_t* fs) {
    // 从第2个簇开始查找（前两个是保留的）
    uint32_t current_cluster = 2;
    uint32_t fat_sectors = fs->bpb.fat_size_32;
    
    uint8_t buffer[512];
    
    // 扫描所有FAT表扇区
    for (uint32_t sector = 0; sector < fat_sectors; sector++) {
        // 读取FAT表扇区
        if (!disk_read(fs->disk, fs->start_lba + fs->fat_start + sector, 1, buffer)) {
            return 0;
        }
        
        // 查找空闲簇(0x00000000)
        for (uint32_t offset = 0; offset < 512; offset += 4) {
            uint32_t fat_entry = *(uint32_t*)&buffer[offset];
            
            if ((fat_entry & 0x0FFFFFFF) == 0) {
                // 找到空闲簇，标记为文件结束
                *(uint32_t*)&buffer[offset] = FAT32_EOC_MARK;
                
                // 写回FAT表
                if (!disk_write(fs->disk, fs->start_lba + fs->fat_start + sector, 1, buffer)) {
                    return 0;
                }
                
                // 更新第二个FAT
                if (fs->bpb.num_fats > 1) {
                    uint32_t fat2_sector = fs->fat_start + sector + fs->bpb.fat_size_32;
                    if (!disk_write(fs->disk, fs->start_lba + fat2_sector, 1, buffer)) {
                        return 0;
                    }
                }
                
                // 返回分配的簇号
                return current_cluster;
            }
            
            current_cluster++;
        }
    }
    
    return 0; // 没有空闲簇
}

// 辅助函数: 释放一个簇链
static int fat32_free_cluster_chain(fat32_t* fs, uint32_t start_cluster) {
    uint32_t current_cluster = start_cluster;
    uint32_t next_cluster;
    
    while (current_cluster >= 2 && current_cluster < FAT32_EOC_MARK) {
        // 获取下一个簇
        next_cluster = fat32_get_next_cluster(fs, current_cluster);
        
        // 将当前簇标记为空闲
        if (!fat32_set_next_cluster(fs, current_cluster, 0)) {
            return 0;
        }
        
        current_cluster = next_cluster;
    }
    
    return 1;
}

// 辅助函数: 簇转换为扇区号
static uint32_t cluster_to_sector(fat32_t* fs, uint32_t cluster) {
    return fs->data_start + (cluster - 2) * fs->bpb.sectors_per_cluster;
}

// 辅助函数: 创建时间和日期
static void fat32_get_current_datetime(uint16_t* date, uint16_t* time) {
    // 在实际系统中会使用RTC
    // 这里简单用一个固定值(2023-01-01 12:00:00)作为示例
    *date = (2023 - 1980) << 9 | 1 << 5 | 1;
    *time = 12 << 11 | 0 << 5 | 0 / 2;
}

// 初始化FAT32文件系统
int fat32_init(fat32_t* fs, disk_t* disk, uint32_t start_lba) {
    // 初始化文件系统结构
    memset(fs, 0, sizeof(fat32_t));
    fs->disk = disk;
    fs->start_lba = start_lba;
    
    // 分配扇区缓冲区
    fs->sector_buffer = fat32_sector_buffer;
    fs->buffer_sector = 0xFFFFFFFF;
    fs->buffer_dirty = 0;
    
    // 读取BPB结构
    if (!disk_read(disk, start_lba, 1, &fs->bpb)) {
        return 0;
    }
    
    // 检查是否为有效的FAT32文件系统
    if (fs->bpb.signature != FAT32_BOOT_SIGNATURE) {
        return 0;
    }
    
    // 检查是否有"FAT32"标记
    if (memcmp(fs->bpb.fs_type, "FAT32   ", 8) != 0) {
        return 0;
    }
    
    // 计算文件系统关键参数
    fs->fat_start = fs->bpb.reserved_sector_count;
    fs->root_dir_first_cluster = fs->bpb.root_cluster;
    fs->data_start = fs->fat_start + (fs->bpb.num_fats * fs->bpb.fat_size_32);
    fs->cluster_size = fs->bpb.sectors_per_cluster * fs->bpb.bytes_per_sector;
    
    // 初始化打开文件列表
    memset(fat32_open_files, 0, sizeof(fat32_open_files));
    
    return 1;
}

// 格式化分区为FAT32
int fat32_format(disk_t* disk, uint32_t start_lba, uint32_t size_sectors, const char* volume_label) {
    if (size_sectors < 65536) {
        // 太小的分区不适合格式化为FAT32
        return 0;
    }
    
    // 创建BPB
    fat32_bpb_t bpb;
    memset(&bpb, 0, sizeof(fat32_bpb_t));
    
    // 设置基本参数
    bpb.jmp_boot[0] = 0xEB;        // 跳转指令
    bpb.jmp_boot[1] = 0x58;
    bpb.jmp_boot[2] = 0x90;
    memcpy(bpb.oem_name, "MSWIN4.1", 8);
    bpb.bytes_per_sector = 512;
    bpb.sectors_per_cluster = 8;   // 4KB簇
    bpb.reserved_sector_count = 32;
    bpb.num_fats = 2;
    bpb.media = 0xF8;             // 固定磁盘
    bpb.sectors_per_track = 63;
    bpb.num_heads = 255;
    bpb.hidden_sectors = start_lba;
    bpb.total_sectors_32 = size_sectors;
    
    // FAT32特有参数
    uint32_t cluster_count = (size_sectors - 32) / 8;  // 扇区总数减去保留扇区，除以每簇扇区数
    bpb.fat_size_32 = (cluster_count * 4 + 511) / 512; // 每个FAT表项4字节
    bpb.ext_flags = 0;
    bpb.fs_version = 0;
    bpb.root_cluster = 2;        // 根目录起始簇号
    bpb.fs_info = 1;             // FSInfo扇区号
    bpb.backup_boot_sector = 6;  // 备份引导扇区
    bpb.drive_number = 0x80;     // 硬盘
    bpb.boot_signature = FAT32_SIGNATURE;
    
    // 设置卷标
    memset(bpb.volume_label, ' ', 11);
    if (volume_label) {
        int len = 0;
        while (volume_label[len] && len < 11) len++;
        memcpy(bpb.volume_label, volume_label, len);
    } else {
        memcpy(bpb.volume_label, "NO NAME    ", 11);
    }
    
    // 设置文件系统类型
    memcpy(bpb.fs_type, "FAT32   ", 8);
    
    // 设置引导扇区签名
    bpb.signature = FAT32_BOOT_SIGNATURE;
    
    // 写入引导扇区
    if (!disk_write(disk, start_lba, 1, &bpb)) {
        return 0;
    }
    
    // 写入FSInfo扇区
    uint8_t fs_info[512];
    memset(fs_info, 0, 512);
    *((uint32_t*)&fs_info[0]) = 0x41615252;  // FSI_LeadSig
    *((uint32_t*)&fs_info[484]) = 0x61417272; // FSI_StrucSig
    *((uint32_t*)&fs_info[488]) = cluster_count - 1; // FSI_Free_Count
    *((uint32_t*)&fs_info[492]) = 3;         // FSI_Nxt_Free (下一个空闲簇)
    *((uint16_t*)&fs_info[510]) = 0xAA55;    // 签名
    
    if (!disk_write(disk, start_lba + 1, 1, fs_info)) {
        return 0;
    }
    
    // 写入FAT表
    uint8_t fat_sector[512];
    memset(fat_sector, 0, 512);
    
    // 设置FAT表的前两个条目
    uint32_t* fat = (uint32_t*)fat_sector;
    fat[0] = 0x0FFFFFF8;  // 介质描述符
    fat[1] = 0x0FFFFFFF;  // EOC标记
    fat[2] = 0x0FFFFFFF;  // 根目录的簇标记为结束
    
    // 写入FAT表的第一个扇区
    if (!disk_write(disk, start_lba + bpb.reserved_sector_count, 1, fat_sector)) {
        return 0;
    }
    
    // 写入第二个FAT表
    if (!disk_write(disk, start_lba + bpb.reserved_sector_count + bpb.fat_size_32, 1, fat_sector)) {
        return 0;
    }
    
    // 清空剩余的FAT表
    memset(fat_sector, 0, 512);
    for (uint32_t i = 1; i < bpb.fat_size_32; i++) {
        if (!disk_write(disk, start_lba + bpb.reserved_sector_count + i, 1, fat_sector)) {
            return 0;
        }
        if (!disk_write(disk, start_lba + bpb.reserved_sector_count + bpb.fat_size_32 + i, 1, fat_sector)) {
            return 0;
        }
    }
    
    // 创建一个空的根目录
    uint32_t root_dir_sector = start_lba + bpb.reserved_sector_count + (bpb.num_fats * bpb.fat_size_32);
    memset(fat_sector, 0, 512);
    
    // 设置卷标目录项
    fat32_dir_entry_t* volume_entry = (fat32_dir_entry_t*)fat_sector;
    memcpy(volume_entry->name, "NO NAME    ", 11); // 默认卷标
    if (volume_label) {
        int len = 0;
        while (volume_label[len] && len < 11) len++;
        memcpy(volume_entry->name, volume_label, len);
    }
    volume_entry->attributes = FAT32_ATTR_VOLUME_ID;
    
    // 设置创建时间和日期
    uint16_t create_date, create_time;
    fat32_get_current_datetime(&create_date, &create_time);
    volume_entry->creation_date = create_date;
    volume_entry->creation_time = create_time;
    volume_entry->last_mod_date = create_date;
    volume_entry->last_mod_time = create_time;
    
    // 写入根目录
    if (!disk_write(disk, root_dir_sector, 1, fat_sector)) {
        return 0;
    }
    
    return 1;
}

// 挂载FAT32文件系统
int fat32_mount(fat32_t* fs, int partition_index) {
    disk_t* disk;
    uint32_t start_lba;
    
    // 获取分区信息
    if (!disk_get_partition_info(partition_index, &disk, &start_lba)) {
        return 0;
    }
    
    // 初始化文件系统
    if (!fat32_init(fs, disk, start_lba)) {
        return 0;
    }
    
    print_string("成功挂载FAT32分区");
    print_newline();
    
    // 输出卷标
    print_string("卷标: ");
    char volume_label[12];
    memcpy(volume_label, fs->bpb.volume_label, 11);
    volume_label[11] = 0;
    print_string(volume_label);
    print_newline();
    
    return 1;
}

// 辅助函数: 将文件名转换为FAT格式 (8.3格式)
static void filename_to_83(const char* filename, char* name83) {
    // 初始化为空格
    memset(name83, ' ', 11);
    
    // 查找扩展名分隔符
    const char* ext = filename;
    while (*ext && *ext != '.') ext++;
    
    // 复制文件名部分 (最多8个字符)
    int name_len = ext - filename;
    if (name_len > 8) name_len = 8;
    
    int i;
    for (i = 0; i < name_len; i++) {
        name83[i] = filename[i];
    }
    
    // 如果有扩展名，复制扩展名部分 (最多3个字符)
    if (*ext == '.') {
        ext++;
        for (i = 0; i < 3 && *ext; i++) {
            name83[i + 8] = *ext++;
        }
    }
    
    // 转换为大写
    for (i = 0; i < 11; i++) {
        if (name83[i] >= 'a' && name83[i] <= 'z') {
            name83[i] = name83[i] - 'a' + 'A';
        }
    }
}

// 辅助函数: 比较文件名
static int compare_filename(const char* name83, const char* filename) {
    char buffer[12];
    filename_to_83(filename, buffer);
    return memcmp(name83, buffer, 11) == 0;
}

// 辅助函数: 查找目录项
static int fat32_find_entry(fat32_t* fs, uint32_t dir_cluster, const char* filename, 
                           fat32_dir_entry_t* entry, uint32_t* entry_sector, uint32_t* entry_offset) {
    uint8_t buffer[512];
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster >= 2 && current_cluster < FAT32_EOC_MARK) {
        // 计算该簇对应的扇区范围
        uint32_t first_sector = cluster_to_sector(fs, current_cluster);
        uint32_t sector_count = fs->bpb.sectors_per_cluster;
        
        // 遍历簇中的所有扇区
        for (uint32_t i = 0; i < sector_count; i++) {
            uint32_t sector = fs->start_lba + first_sector + i;
            
            // 读取扇区
            if (!disk_read(fs->disk, sector, 1, buffer)) {
                return 0;
            }
            
            // 遍历扇区中的所有目录项
            for (uint32_t offset = 0; offset < 512; offset += sizeof(fat32_dir_entry_t)) {
                fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)(buffer + offset);
                
                // 检查是否为空目录项或已删除目录项
                if (dir_entry->name[0] == 0x00) {
                    // 已达到目录尾部
                    return 0;
                }
                
                if (dir_entry->name[0] == 0xE5) {
                    // 已删除的目录项
                    continue;
                }
                
                // 跳过卷标和长文件名项
                if (dir_entry->attributes == FAT32_ATTR_VOLUME_ID ||
                    (dir_entry->attributes & FAT32_ATTR_LONG_NAME_MASK) == FAT32_ATTR_LONG_NAME) {
                    continue;
                }
                
                // 检查文件名是否匹配
                if (compare_filename((char*)dir_entry->name, filename)) {
                    // 找到匹配的目录项
                    if (entry) *entry = *dir_entry;
                    if (entry_sector) *entry_sector = sector;
                    if (entry_offset) *entry_offset = offset;
                    return 1;
                }
            }
        }
        
        // 获取下一个簇
        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }
    
    return 0; // 未找到
}

// 辅助函数: 查找空闲目录项
static int fat32_find_free_entry(fat32_t* fs, uint32_t dir_cluster, 
                               fat32_dir_entry_t* entry, uint32_t* entry_sector, uint32_t* entry_offset) {
    uint8_t buffer[512];
    uint32_t current_cluster = dir_cluster;
    uint32_t last_cluster = dir_cluster;
    
    while (current_cluster >= 2 && current_cluster < FAT32_EOC_MARK) {
        // 计算该簇对应的扇区范围
        uint32_t first_sector = cluster_to_sector(fs, current_cluster);
        uint32_t sector_count = fs->bpb.sectors_per_cluster;
        
        // 遍历簇中的所有扇区
        for (uint32_t i = 0; i < sector_count; i++) {
            uint32_t sector = fs->start_lba + first_sector + i;
            
            // 读取扇区
            if (!disk_read(fs->disk, sector, 1, buffer)) {
                return 0;
            }
            
            // 遍历扇区中的所有目录项
            for (uint32_t offset = 0; offset < 512; offset += sizeof(fat32_dir_entry_t)) {
                fat32_dir_entry_t* dir_entry = (fat32_dir_entry_t*)(buffer + offset);
                
                // 检查是否为空目录项或已删除目录项
                if (dir_entry->name[0] == 0x00 || dir_entry->name[0] == 0xE5) {
                    // 找到空闲目录项
                    if (entry) memset(entry, 0, sizeof(fat32_dir_entry_t));
                    if (entry_sector) *entry_sector = sector;
                    if (entry_offset) *entry_offset = offset;
                    return 1;
                }
            }
        }
        
        last_cluster = current_cluster;
        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }
    
    // 需要增加一个新簇来存储更多的目录项
    uint32_t new_cluster = fat32_allocate_cluster(fs);
    if (new_cluster == 0) {
        return 0; // 无法分配新簇
    }
    
    // 设置上一个簇指向新簇
    if (!fat32_set_next_cluster(fs, last_cluster, new_cluster)) {
        return 0;
    }
    
    // 清空新簇
    uint32_t first_sector = cluster_to_sector(fs, new_cluster);
    for (uint32_t i = 0; i < fs->bpb.sectors_per_cluster; i++) {
        fill_sector(buffer, 0);
        if (!disk_write(fs->disk, fs->start_lba + first_sector + i, 1, buffer)) {
            return 0;
        }
    }
    
    // 使用新簇的第一个目录项
    if (entry) memset(entry, 0, sizeof(fat32_dir_entry_t));
    if (entry_sector) *entry_sector = fs->start_lba + first_sector;
    if (entry_offset) *entry_offset = 0;
    
    return 1;
}

// 辅助函数: 拆分路径
static int fat32_split_path(const char* path, char* dirname, char* basename) {
    // 找到最后一个斜杠
    const char* last_slash = path;
    const char* p = path;
    
    while (*p) {
        if (*p == '/' || *p == '\\') {
            last_slash = p;
        }
        p++;
    }
    
    // 如果整个路径没有斜杠，则目录为根目录
    if (last_slash == path) {
        if (dirname) *dirname = 0;
        if (basename) strcpy(basename, path);
        return 1;
    }
    
    // 提取目录名和文件名
    if (dirname) {
        int dir_len = last_slash - path;
        memcpy(dirname, path, dir_len);
        dirname[dir_len] = 0;
    }
    
    if (basename) {
        strcpy(basename, last_slash + 1);
    }
    
    return 1;
}

// 辅助函数: 查找目录
static uint32_t fat32_find_directory(fat32_t* fs, const char* path) {
    if (path == NULL || *path == 0 || (*path == '/' && *(path+1) == 0)) {
        return fs->root_dir_first_cluster; // 根目录
    }
    
    // 从根目录开始查找
    uint32_t current_dir = fs->root_dir_first_cluster;
    
    // 拆分路径成各个目录名
    char path_copy[FAT32_MAX_PATH_LEN];
    strcpy(path_copy, path);
    
    char* token = path_copy;
    char* next_token = NULL;
    
    // 跳过开头的斜杠
    if (*token == '/' || *token == '\\') {
        token++;
    }
    
    // 遍历路径的每个部分
    while (token && *token) {
        // 查找下一个斜杠
        next_token = token;
        while (*next_token && *next_token != '/' && *next_token != '\\') next_token++;
        
        // 如果找到斜杠，将其替换为NULL字符并移动到下一个位置
        if (*next_token) {
            *next_token = 0;
            next_token++;
        } else {
            next_token = NULL;
        }
        
        // 在当前目录中查找子目录
        fat32_dir_entry_t entry;
        if (!fat32_find_entry(fs, current_dir, token, &entry, NULL, NULL)) {
            return 0; // 目录不存在
        }
        
        // 检查是否为目录
        if (!(entry.attributes & FAT32_ATTR_DIRECTORY)) {
            return 0; // 不是目录
        }
        
        // 更新当前目录为找到的子目录
        current_dir = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
        
        // 继续下一个目录
        token = next_token;
    }
    
    return current_dir;
}

// 文件操作: 打开文件
fat32_file_t* fat32_fopen(fat32_t* fs, const char* path, const char* mode) {
    // 解析模式
    int create = 0;
    if (strchr(mode, 'w') || strchr(mode, 'a')) {
        create = 1;
    }
    
    // 拆分路径
    char dirname[FAT32_MAX_PATH_LEN];
    char basename[FAT32_MAX_FILENAME];
    fat32_split_path(path, dirname, basename);
    
    // 找到父目录
    uint32_t parent_dir = fat32_find_directory(fs, dirname);
    if (parent_dir == 0) {
        return NULL; // 目录不存在
    }
    
    // 在目录中查找文件
    fat32_dir_entry_t entry;
    uint32_t entry_sector;
    uint32_t entry_offset;
    int file_exists = fat32_find_entry(fs, parent_dir, basename, &entry, &entry_sector, &entry_offset);
    
    // 如果文件不存在且不需要创建，则返回错误
    if (!file_exists && !create) {
        return NULL;
    }
    
    // 如果文件不存在但需要创建
    if (!file_exists && create) {
        // 查找空闲目录项
        if (!fat32_find_free_entry(fs, parent_dir, &entry, &entry_sector, &entry_offset)) {
            return NULL; // 无法创建文件
        }
        
        // 分配第一个簇
        uint32_t first_cluster = fat32_allocate_cluster(fs);
        if (first_cluster == 0) {
            return NULL; // 无法分配簇
        }
        
        // 初始化新目录项
        filename_to_83(basename, (char*)entry.name);
        entry.attributes = FAT32_ATTR_ARCHIVE;
        entry.first_cluster_high = (first_cluster >> 16) & 0xFFFF;
        entry.first_cluster_low = first_cluster & 0xFFFF;
        entry.file_size = 0;
        
        // 设置创建时间和日期
        uint16_t create_date, create_time;
        fat32_get_current_datetime(&create_date, &create_time);
        entry.creation_date = create_date;
        entry.creation_time = create_time;
        entry.last_mod_date = create_date;
        entry.last_mod_time = create_time;
        entry.last_access_date = create_date;
        
        // 写入目录项
        uint8_t buffer[512];
        if (!disk_read(fs->disk, entry_sector, 1, buffer)) {
            return NULL;
        }
        
        memcpy(buffer + entry_offset, &entry, sizeof(fat32_dir_entry_t));
        
        if (!disk_write(fs->disk, entry_sector, 1, buffer)) {
            return NULL;
        }
    }
    
    // 查找空闲文件句柄
    fat32_file_t* file = NULL;
    for (int i = 0; i < FAT32_MAX_OPEN_FILES; i++) {
        if (fat32_open_files[i].fs == NULL) {
            file = &fat32_open_files[i];
            break;
        }
    }
    
    if (file == NULL) {
        return NULL; // 打开的文件太多
    }
    
    // 初始化文件句柄
    file->fs = fs;
    file->first_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    file->current_cluster = file->first_cluster;
    file->size = entry.file_size;
    file->position = 0;
    file->entry = entry;
    file->parent_cluster = parent_dir;
    file->dir_entry_sector = entry_sector;
    file->dir_entry_offset = entry_offset;
    
    // 如果是追加模式，定位到文件末尾
    if (strchr(mode, 'a')) {
        fat32_fseek(file, 0, 2); // SEEK_END
    }
    
    // 如果是写模式，截断文件
    if (strchr(mode, 'w') && file_exists) {
        // 释放原有的簇链
        if (file->first_cluster >= 2) {
            fat32_free_cluster_chain(fs, file->first_cluster);
        }
        
        // 分配新的第一个簇
        file->first_cluster = fat32_allocate_cluster(fs);
        if (file->first_cluster == 0) {
            file->fs = NULL; // 标记为关闭
            return NULL;
        }
        
        file->current_cluster = file->first_cluster;
        file->size = 0;
        file->position = 0;
        
        // 更新目录项
        entry.first_cluster_high = (file->first_cluster >> 16) & 0xFFFF;
        entry.first_cluster_low = file->first_cluster & 0xFFFF;
        entry.file_size = 0;
        
        // 写入更新后的目录项
        uint8_t buffer[512];
        if (!disk_read(fs->disk, entry_sector, 1, buffer)) {
            file->fs = NULL;
            return NULL;
        }
        
        memcpy(buffer + entry_offset, &entry, sizeof(fat32_dir_entry_t));
        
        if (!disk_write(fs->disk, entry_sector, 1, buffer)) {
            file->fs = NULL;
            return NULL;
        }
    }
    
    return file;
}

// 文件操作: 关闭文件
int fat32_fclose(fat32_file_t* file) {
    if (file == NULL || file->fs == NULL) {
        return 0;
    }
    
    // 更新目录项中的文件大小
    if (file->size != file->entry.file_size) {
        uint8_t buffer[512];
        if (!disk_read(file->fs->disk, file->dir_entry_sector, 1, buffer)) {
            return 0;
        }
        
        fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(buffer + file->dir_entry_offset);
        entry->file_size = file->size;
        
        // 更新修改时间和日期
        uint16_t mod_date, mod_time;
        fat32_get_current_datetime(&mod_date, &mod_time);
        entry->last_mod_date = mod_date;
        entry->last_mod_time = mod_time;
        
        if (!disk_write(file->fs->disk, file->dir_entry_sector, 1, buffer)) {
            return 0;
        }
    }
    
    // 标记文件句柄为空闲
    file->fs = NULL;
    
    return 1;
} 