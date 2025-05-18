#ifndef DISK_H
#define DISK_H

#include <stdint.h>

// ATA/IDE驱动定义
#define ATA_PRIMARY      0x1F0
#define ATA_SECONDARY    0x170

#define ATA_DATA         0x00
#define ATA_ERROR        0x01
#define ATA_FEATURES     0x01
#define ATA_SECTOR_COUNT 0x02
#define ATA_LBA_LOW      0x03
#define ATA_LBA_MID      0x04
#define ATA_LBA_HIGH     0x05
#define ATA_DEVICE       0x06
#define ATA_STATUS       0x07
#define ATA_COMMAND      0x07

#define ATA_PRIMARY_DCR  0x3F6
#define ATA_SECONDARY_DCR 0x376

// ATA命令
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_IDENTIFY          0xEC
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA

// ATA状态标志
#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

// 设备/磁头寄存器值
#define ATA_MASTER     0xA0
#define ATA_SLAVE      0xB0

// 分区表项结构体
typedef struct {
    uint8_t bootable;           // 0x80 表示可引导分区
    uint8_t start_head;         // 起始磁头
    uint16_t start_sector_cyl;  // 起始扇区和柱面
    uint8_t system_id;          // 分区类型标识
    uint8_t end_head;           // 结束磁头
    uint16_t end_sector_cyl;    // 结束扇区和柱面
    uint32_t lba_start;         // 起始LBA地址
    uint32_t sector_count;      // 扇区数
} __attribute__((packed)) partition_entry_t;

// MBR结构体
typedef struct {
    uint8_t bootstrap[446];     // 引导代码
    partition_entry_t partitions[4]; // 4个主分区表项
    uint16_t signature;         // 0xAA55 MBR签名
} __attribute__((packed)) mbr_t;

// 磁盘设备类型
typedef struct {
    uint16_t base;             // 基址
    uint8_t device;            // 主/从
    uint8_t type;              // ATA/ATAPI等
    uint16_t signature;        // 签名
    uint16_t capabilities;     // 能力
    uint32_t command_sets;     // 支持的命令集
    uint32_t size;             // 扇区数量
    uint8_t model[41];         // 型号字符串
    int has_partitions;        // 是否有分区表
    partition_entry_t partitions[4]; // 分区信息
} disk_t;

// 分区类型定义
#define PART_TYPE_EMPTY       0x00
#define PART_TYPE_FAT12       0x01
#define PART_TYPE_FAT16_SM    0x04
#define PART_TYPE_EXTENDED    0x05
#define PART_TYPE_FAT16       0x06
#define PART_TYPE_NTFS        0x07
#define PART_TYPE_FAT32       0x0B
#define PART_TYPE_FAT32_LBA   0x0C
#define PART_TYPE_FAT16_LBA   0x0E
#define PART_TYPE_EXTENDED_LBA 0x0F
#define PART_TYPE_LINUX       0x83

// 初始化磁盘驱动
void disk_init();

// 检测磁盘
int disk_detect(uint16_t base, uint8_t device);

// 读取扇区 (LBA模式)
int disk_read(disk_t* disk, uint32_t lba, uint8_t sectors, void* buffer);

// 写入扇区 (LBA模式)
int disk_write(disk_t* disk, uint32_t lba, uint8_t sectors, const void* buffer);

// 获取磁盘信息
int disk_identify(disk_t* disk);

// 读取分区表
int disk_read_partitions(disk_t* disk);

// 刷新磁盘缓存
int disk_flush(disk_t* disk);

// 获取主磁盘
disk_t* disk_get_primary();

// 获取次磁盘
disk_t* disk_get_secondary();

// 获取分区所在磁盘和LBA偏移
int disk_get_partition_info(int partition_index, disk_t** disk, uint32_t* start_lba);

#endif // DISK_H 