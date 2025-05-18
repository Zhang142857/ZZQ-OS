#ifndef NTFS_H
#define NTFS_H

#include <stdint.h>

// NTFS 超级块结构（简化版）
typedef struct {
    uint8_t jump[3];             // 跳转指令
    char oem_id[8];              // "NTFS    "
    uint16_t bytes_per_sector;   // 每扇区字节数
    uint8_t sectors_per_cluster; // 每簇扇区数
    uint16_t reserved_sectors;   // 保留扇区数
    uint8_t num_fats;            // FAT表数量
    uint16_t root_entries;       // 根目录项数
    uint16_t total_sectors_16;   // 总扇区数(16位)
    uint8_t media_type;          // 媒体描述符
    uint16_t sectors_per_fat;    // 每FAT扇区数
    uint16_t sectors_per_track;  // 每磁道扇区数
    uint16_t num_heads;          // 磁头数
    uint32_t hidden_sectors;     // 隐藏扇区数
    uint32_t total_sectors_32;   // 总扇区数(32位)
    uint32_t sectors_per_fat_32; // 每FAT扇区数(FAT32)
    uint64_t mft_cluster;        // MFT起始簇号
    uint64_t mft_mirror_cluster; // MFT镜像起始簇号
    int8_t clusters_per_mft_record; // 每MFT记录簇数
    uint8_t reserved[3];         // 保留
    int8_t clusters_per_index_record; // 每索引记录簇数
    uint8_t reserved2[3];        // 保留
    uint64_t volume_serial;      // 卷序列号
    uint32_t checksum;           // 校验和
} __attribute__((packed)) ntfs_boot_sector_t;

// MFT记录头
typedef struct {
    char signature[4];           // "FILE"
    uint16_t update_seq_offset;  // 更新序列偏移
    uint16_t update_seq_size;    // 更新序列大小
    uint64_t logfile_seq_num;    // 日志文件序列号
    uint16_t sequence_number;    // 序列号
    uint16_t hard_link_count;    // 硬链接数
    uint16_t attrs_offset;       // 属性偏移
    uint16_t flags;              // 标志位
    uint32_t bytes_used;         // 已用字节数
    uint32_t bytes_allocated;    // 分配字节数
    uint64_t base_record;        // 基本记录指针
    uint16_t next_attr_id;       // 下一个属性ID
    uint16_t padding;            // 对齐填充
    uint32_t record_number;      // 记录号
} __attribute__((packed)) mft_record_header_t;

// 属性类型
typedef enum {
    STANDARD_INFORMATION = 0x10,
    ATTRIBUTE_LIST = 0x20,
    FILE_NAME = 0x30,
    OBJECT_ID = 0x40,
    SECURITY_DESCRIPTOR = 0x50,
    VOLUME_NAME = 0x60,
    VOLUME_INFORMATION = 0x70,
    DATA = 0x80,
    INDEX_ROOT = 0x90,
    INDEX_ALLOCATION = 0xA0,
    BITMAP = 0xB0,
    REPARSE_POINT = 0xC0,
    EA_INFORMATION = 0xD0,
    EA = 0xE0,
    PROPERTY_SET = 0xF0,
    LOGGED_UTILITY_STREAM = 0x100
} ntfs_attribute_type;

// 属性头
typedef struct {
    uint32_t type;               // 属性类型
    uint32_t length;             // 属性长度
    uint8_t non_resident_flag;   // 是否非常驻
    uint8_t name_length;         // 名称长度
    uint16_t name_offset;        // 名称偏移
    uint16_t flags;              // 标志位
    uint16_t attribute_id;       // 属性ID
} __attribute__((packed)) ntfs_attribute_header_t;

// 常驻属性头
typedef struct {
    ntfs_attribute_header_t header;
    uint32_t value_length;       // 值长度
    uint16_t value_offset;       // 值偏移
    uint16_t flags;              // 标志位
} __attribute__((packed)) ntfs_resident_attribute_t;

// 非常驻属性头
typedef struct {
    ntfs_attribute_header_t header;
    uint64_t starting_vcn;       // 起始VCN
    uint64_t last_vcn;           // 结束VCN
    uint16_t run_list_offset;    // 运行列表偏移
    uint16_t compression_unit;   // 压缩单元大小
    uint32_t padding;            // 填充
    uint64_t allocated_size;     // 分配大小
    uint64_t data_size;          // 数据大小
    uint64_t initialized_size;   // 初始化大小
    uint64_t compressed_size;    // 压缩后大小(仅压缩文件)
} __attribute__((packed)) ntfs_non_resident_attribute_t;

// 文件名属性
typedef struct {
    uint64_t parent_directory;   // 父目录参考
    uint64_t creation_time;      // 创建时间
    uint64_t modification_time;  // 修改时间
    uint64_t mft_modification_time; // MFT修改时间
    uint64_t access_time;        // 访问时间
    uint64_t allocated_size;     // 分配大小
    uint64_t data_size;          // 数据大小
    uint32_t file_attributes;    // 文件属性
    uint32_t ea_and_reparse;     // EA和重解析
    uint8_t filename_length;     // 文件名长度
    uint8_t filename_namespace;  // 文件名命名空间
    uint16_t filename[1];        // 文件名(UTF-16)
} __attribute__((packed)) ntfs_filename_attribute_t;

// NTFS 文件
typedef struct {
    uint64_t inode;              // 索引节点号
    uint16_t attributes;         // 文件属性
    char name[256];              // 文件名(ASCII)
    uint64_t size;               // 文件大小
    uint64_t creation_time;      // 创建时间
    uint64_t modification_time;  // 修改时间
    uint64_t access_time;        // 访问时间
} ntfs_file_t;

// NTFS 文件系统
typedef struct {
    ntfs_boot_sector_t boot_sector;
    uint32_t bytes_per_cluster;
    uint64_t mft_record_size;
    void* mft_cache;
    uint8_t mounted;
    uint64_t volume_size;
} ntfs_fs_t;

// 主要功能函数声明
int ntfs_init(ntfs_fs_t* fs, uint32_t disk_sector);
int ntfs_mount(ntfs_fs_t* fs);
int ntfs_unmount(ntfs_fs_t* fs);
int ntfs_find_file(ntfs_fs_t* fs, const char* path, ntfs_file_t* file);
int ntfs_read_file(ntfs_fs_t* fs, ntfs_file_t* file, uint64_t offset, uint32_t size, void* buffer);
int ntfs_write_file(ntfs_fs_t* fs, ntfs_file_t* file, uint64_t offset, uint32_t size, const void* buffer);
int ntfs_create_file(ntfs_fs_t* fs, const char* path, ntfs_file_t* file);
int ntfs_delete_file(ntfs_fs_t* fs, const char* path);
int ntfs_list_directory(ntfs_fs_t* fs, const char* path, ntfs_file_t** files, uint32_t* count);
int ntfs_format(uint32_t disk_sector, uint64_t size);

#endif // NTFS_H 