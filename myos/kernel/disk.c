#include "disk.h"
#include "string.h"

// 外部函数声明
extern void print_string(const char* str);
extern void print_newline(void);
extern void int_to_string(int num, char* str);

// 定义两个主要磁盘
static disk_t primary_disk;
static disk_t secondary_disk;

// 读端口函数（8位、16位读）
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 读多个字
static inline void insw(uint16_t port, void* addr, uint32_t count) {
    asm volatile("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

// 写端口函数（8位、16位写）
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

// 写多个字
static inline void outsw(uint16_t port, const void* addr, uint32_t count) {
    asm volatile("rep outsw" : "+S"(addr), "+c"(count) : "d"(port));
}

// 等待BSY标志清除
static void ata_wait_bsy(uint16_t base) {
    while (inb(base + ATA_STATUS) & ATA_SR_BSY);
}

// 等待DRQ标志设置
static int ata_wait_drq(uint16_t base) {
    uint8_t status;
    int timeout = 1000; // 超时防止死锁
    
    while (--timeout) {
        status = inb(base + ATA_STATUS);
        if (status & ATA_SR_ERR) return -1;  // 错误
        if (status & ATA_SR_DRQ) return 0;  // 就绪
    }
    
    return -1; // 超时
}

// 初始化磁盘驱动
void disk_init() {
    // 初始化主磁盘结构
    memset(&primary_disk, 0, sizeof(disk_t));
    primary_disk.base = ATA_PRIMARY;
    primary_disk.device = ATA_MASTER;
    
    // 初始化次磁盘结构
    memset(&secondary_disk, 0, sizeof(disk_t));
    secondary_disk.base = ATA_SECONDARY;
    secondary_disk.device = ATA_MASTER;
    
    // 检测所有磁盘
    print_string("正在检测磁盘...");
    print_newline();
    
    // 检测主主磁盘
    if (disk_identify(&primary_disk)) {
        char size_str[16];
        int_to_string(primary_disk.size / 2048, size_str); // 显示MB
        print_string("发现主IDE磁盘: ");
        print_string((char*)primary_disk.model);
        print_string(", 容量: ");
        print_string(size_str);
        print_string("MB");
        print_newline();
        
        // 读取主磁盘分区表
        if (disk_read_partitions(&primary_disk)) {
            print_string("已读取分区表");
            print_newline();
        }
    }
    
    // 检测次主磁盘
    if (disk_identify(&secondary_disk)) {
        char size_str[16];
        int_to_string(secondary_disk.size / 2048, size_str); // 显示MB
        print_string("发现次IDE磁盘: ");
        print_string((char*)secondary_disk.model);
        print_string(", 容量: ");
        print_string(size_str);
        print_string("MB");
        print_newline();
        
        // 读取次磁盘分区表
        if (disk_read_partitions(&secondary_disk)) {
            print_string("已读取分区表");
            print_newline();
        }
    }
}

// 获取磁盘信息
int disk_identify(disk_t* disk) {
    uint16_t base = disk->base;
    uint8_t device = disk->device;
    uint16_t buffer[256];
    
    // 1. 选择驱动器
    outb(base + ATA_DEVICE, device);
    
    // 2. 发送NUL字节到控制寄存器以清除任何挂起的中断
    outb(base + ATA_PRIMARY_DCR, 0);
    
    // 3. 发送IDENTIFY命令
    outb(base + ATA_COMMAND, ATA_CMD_IDENTIFY);
    
    // 4. 检查驱动器是否存在
    if (inb(base + ATA_STATUS) == 0) {
        return 0; // 驱动器不存在
    }
    
    // 5. 等待操作完成
    ata_wait_bsy(base);
    if (ata_wait_drq(base) < 0) {
        return 0; // 等待超时或错误
    }
    
    // 6. 读取扇区数据
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(base + ATA_DATA);
    }
    
    // 7. 提取磁盘信息
    disk->signature = buffer[0];
    disk->capabilities = buffer[49];
    disk->command_sets = ((uint32_t)buffer[83] << 16) | buffer[82];
    
    // 计算磁盘大小
    if (disk->command_sets & (1 << 26)) {
        // 48位LBA支持，使用扩展扇区数
        disk->size = ((uint32_t)buffer[103] << 16) | buffer[102];
        disk->size = ((uint32_t)buffer[101] << 16) | buffer[100];
    } else {
        // 传统28位LBA
        disk->size = ((uint32_t)buffer[61] << 16) | buffer[60];
    }
    
    // 提取型号名称
    char* model = (char*)&disk->model;
    for (int i = 0; i < 40; i += 2) {
        model[i] = (char)buffer[27 + i/2] >> 8;
        model[i+1] = (char)buffer[27 + i/2];
    }
    model[40] = 0; // 确保以空字符结尾
    
    // 处理字符串，移除尾部空格
    int len = 40;
    while (len > 0 && model[len-1] == ' ') {
        model[--len] = 0;
    }
    
    return 1; // 成功
}

// 读取分区表
int disk_read_partitions(disk_t* disk) {
    uint8_t buffer[512];
    mbr_t* mbr = (mbr_t*)buffer;
    
    // 读取MBR扇区
    if (!disk_read(disk, 0, 1, buffer)) {
        return 0;
    }
    
    // 检查MBR签名
    if (mbr->signature != 0xAA55) {
        return 0; // 无效MBR
    }
    
    // 复制分区表
    for (int i = 0; i < 4; i++) {
        disk->partitions[i] = mbr->partitions[i];
        
        // 检查是否为有效分区
        if (disk->partitions[i].system_id != PART_TYPE_EMPTY) {
            disk->has_partitions = 1;
            
            // 输出分区信息
            char lba_str[16], size_str[16];
            int_to_string(disk->partitions[i].lba_start, lba_str);
            int_to_string(disk->partitions[i].sector_count / 2048, size_str); // MB
            
            print_string("分区 ");
            char idx[2] = {'1' + i, 0};
            print_string(idx);
            print_string(": 类型=0x");
            
            // 输出十六进制的分区类型
            char hex[3];
            hex[0] = "0123456789ABCDEF"[(disk->partitions[i].system_id >> 4) & 0x0F];
            hex[1] = "0123456789ABCDEF"[disk->partitions[i].system_id & 0x0F];
            hex[2] = 0;
            print_string(hex);
            
            print_string(", LBA=");
            print_string(lba_str);
            print_string(", 大小=");
            print_string(size_str);
            print_string("MB");
            print_newline();
        }
    }
    
    return disk->has_partitions;
}

// 从磁盘读取扇区 (LBA模式)
int disk_read(disk_t* disk, uint32_t lba, uint8_t sectors, void* buffer) {
    uint16_t base = disk->base;
    uint8_t device = disk->device;
    
    // 检查参数
    if (sectors == 0) return 0;
    if (lba + sectors > disk->size) return 0;
    
    // 选择驱动器和发送相关参数
    outb(base + ATA_DEVICE, (device & 0xE0) | ((lba >> 24) & 0x0F));
    
    // 等待设备就绪
    ata_wait_bsy(base);
    
    // 发送扇区计数
    outb(base + ATA_SECTOR_COUNT, sectors);
    
    // 发送LBA地址 (低24位)
    outb(base + ATA_LBA_LOW, lba & 0xFF);
    outb(base + ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(base + ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // 发送读扇区命令
    outb(base + ATA_COMMAND, ATA_CMD_READ_PIO);
    
    // 读取数据
    uint16_t* buf = (uint16_t*)buffer;
    for (int s = 0; s < sectors; s++) {
        // 等待DRQ标志
        if (ata_wait_drq(base) < 0) {
            return 0; // 错误
        }
        
        // 读取一个扇区的数据
        for (int i = 0; i < 256; i++) {
            buf[i] = inw(base + ATA_DATA);
        }
        
        // 更新缓冲区指针
        buf += 256;
    }
    
    return 1; // 成功
}

// 向磁盘写入扇区 (LBA模式)
int disk_write(disk_t* disk, uint32_t lba, uint8_t sectors, const void* buffer) {
    uint16_t base = disk->base;
    uint8_t device = disk->device;
    
    // 检查参数
    if (sectors == 0) return 0;
    if (lba + sectors > disk->size) return 0;
    
    // 选择驱动器和发送相关参数
    outb(base + ATA_DEVICE, (device & 0xE0) | ((lba >> 24) & 0x0F));
    
    // 等待设备就绪
    ata_wait_bsy(base);
    
    // 发送扇区计数
    outb(base + ATA_SECTOR_COUNT, sectors);
    
    // 发送LBA地址 (低24位)
    outb(base + ATA_LBA_LOW, lba & 0xFF);
    outb(base + ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(base + ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // 发送写扇区命令
    outb(base + ATA_COMMAND, ATA_CMD_WRITE_PIO);
    
    // 写入数据
    const uint16_t* buf = (const uint16_t*)buffer;
    for (int s = 0; s < sectors; s++) {
        // 等待DRQ标志
        if (ata_wait_drq(base) < 0) {
            return 0; // 错误
        }
        
        // 写入一个扇区的数据
        for (int i = 0; i < 256; i++) {
            outw(base + ATA_DATA, buf[i]);
        }
        
        // 更新缓冲区指针
        buf += 256;
    }
    
    // 刷新缓存
    outb(base + ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy(base);
    
    return 1; // 成功
}

// 刷新磁盘缓存
int disk_flush(disk_t* disk) {
    uint16_t base = disk->base;
    uint8_t device = disk->device;
    
    // 选择驱动器
    outb(base + ATA_DEVICE, device);
    
    // 等待设备就绪
    ata_wait_bsy(base);
    
    // 发送缓存刷新命令
    outb(base + ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    
    // 等待操作完成
    ata_wait_bsy(base);
    
    return 1; // 成功
}

// 获取主磁盘
disk_t* disk_get_primary() {
    return &primary_disk;
}

// 获取次磁盘
disk_t* disk_get_secondary() {
    return &secondary_disk;
}

// 获取分区所在磁盘和LBA偏移
int disk_get_partition_info(int partition_index, disk_t** disk, uint32_t* start_lba) {
    // 先检查主磁盘的分区
    if (primary_disk.has_partitions) {
        int primary_part_count = 0;
        
        for (int i = 0; i < 4; i++) {
            if (primary_disk.partitions[i].system_id != PART_TYPE_EMPTY) {
                if (primary_part_count == partition_index) {
                    *disk = &primary_disk;
                    *start_lba = primary_disk.partitions[i].lba_start;
                    return 1;
                }
                primary_part_count++;
            }
        }
        
        // 检查是否需要查找次磁盘
        if (secondary_disk.has_partitions) {
            partition_index -= primary_part_count;
            
            for (int i = 0; i < 4; i++) {
                if (secondary_disk.partitions[i].system_id != PART_TYPE_EMPTY) {
                    if (partition_index == 0) {
                        *disk = &secondary_disk;
                        *start_lba = secondary_disk.partitions[i].lba_start;
                        return 1;
                    }
                    partition_index--;
                }
            }
        }
    }
    // 如果主磁盘没有分区，直接检查次磁盘
    else if (secondary_disk.has_partitions) {
        for (int i = 0; i < 4; i++) {
            if (secondary_disk.partitions[i].system_id != PART_TYPE_EMPTY) {
                if (partition_index == 0) {
                    *disk = &secondary_disk;
                    *start_lba = secondary_disk.partitions[i].lba_start;
                    return 1;
                }
                partition_index--;
            }
        }
    }
    
    return 0; // 未找到分区
} 