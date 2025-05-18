#include "ntfs.h"
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
static ntfs_fs_t ntfs_fs;
static uint8_t disk_buffer[512];

// 获取NTFS文件系统实例的函数
ntfs_fs_t* get_ntfs_fs(void) {
    return &ntfs_fs;
}

// 从外部导入的函数
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

// 辅助函数
static void insl(unsigned short port, void* addr, unsigned int count) {
    // 从ATA控制器端口读取数据
    unsigned int* buffer = (unsigned int*)addr;
    for (unsigned int i = 0; i < count; i++) {
        // 每次读取32位数据
        buffer[i] = inb(port) | (inb(port) << 8) | (inb(port) << 16) | (inb(port) << 24);
    }
}

static void outsl(unsigned short port, const void* addr, unsigned int count) {
    // 向ATA控制器端口写入数据
    const unsigned int* buffer = (const unsigned int*)addr;
    for (unsigned int i = 0; i < count; i++) {
        // 每次写入32位数据
        outb(port, buffer[i] & 0xFF);
        outb(port, (buffer[i] >> 8) & 0xFF);
        outb(port, (buffer[i] >> 16) & 0xFF);
        outb(port, (buffer[i] >> 24) & 0xFF);
    }
}

// 读取一个扇区 - 简化版
static int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    int i;
    
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
    for (i = 0; i < 512; i += 2) {
        // 读取16位数据
        uint8_t low = inb(ATA_DATA);
        uint8_t high = inb(ATA_DATA);
        
        buffer[i] = low;
        buffer[i+1] = high;
    }
    
    // 等待操作完成
    while (inb(ATA_STATUS) & ATA_STATUS_BSY);
    
    // 检查错误
    uint8_t status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR) {
        return -1;
    }
    
    return 0;
}

// 写入一个扇区 - 简化版
static int ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    int i;
    
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
    for (i = 0; i < 512; i += 2) {
        uint16_t data = buffer[i] | (buffer[i+1] << 8);
        
        // 使用16位数据传输，适用于QEMU的ATA模拟
        outb(ATA_DATA, data & 0xFF);
        outb(ATA_DATA, (data >> 8) & 0xFF);
    }
    
    // 等待操作完成
    while ((inb(ATA_STATUS) & (ATA_STATUS_BSY | ATA_STATUS_DRQ)));
    
    // 检查错误
    uint8_t status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR) {
        return -1;
    }
    
    return 0;
}

// 初始化NTFS文件系统
int ntfs_init(ntfs_fs_t* fs, uint32_t disk_sector) {
    // 读取引导扇区
    if (ata_read_sector(disk_sector, disk_buffer) != 0) {
        return -1;
    }
    
    // 验证NTFS签名
    if (disk_buffer[3] != 'N' || disk_buffer[4] != 'T' || 
        disk_buffer[5] != 'F' || disk_buffer[6] != 'S') {
        return -2; // 不是NTFS分区
    }
    
    // 填充超级块
    memcpy(&fs->boot_sector, disk_buffer, sizeof(ntfs_boot_sector_t));
    
    // 计算基本参数
    fs->bytes_per_cluster = fs->boot_sector.bytes_per_sector * fs->boot_sector.sectors_per_cluster;
    
    // 计算MFT记录大小
    if (fs->boot_sector.clusters_per_mft_record >= 0) {
        fs->mft_record_size = fs->boot_sector.clusters_per_mft_record * fs->bytes_per_cluster;
    } else {
        fs->mft_record_size = 1 << (-fs->boot_sector.clusters_per_mft_record);
    }
    
    // 初始化成功
    fs->mounted = 0;
    
    return 0;
}

// 挂载NTFS文件系统
int ntfs_mount(ntfs_fs_t* fs) {
    if (fs->mounted) {
        return 0; // 已经挂载
    }
    
    // 分配MFT缓存
    fs->mft_cache = (void*)0x100000; // 分配1MB内存用于MFT缓存
    
    // 标记为已挂载
    fs->mounted = 1;
    
    return 0;
}

// 卸载NTFS文件系统
int ntfs_unmount(ntfs_fs_t* fs) {
    if (!fs->mounted) {
        return 0; // 已经卸载
    }
    
    // 标记为已卸载
    fs->mounted = 0;
    
    return 0;
}

// 读取MFT记录
static int read_mft_record(ntfs_fs_t* fs, uint64_t record_num, void* buffer) {
    uint64_t cluster = fs->boot_sector.mft_cluster;
    uint64_t sector = cluster * fs->boot_sector.sectors_per_cluster;
    uint32_t sectors_per_record = fs->mft_record_size / fs->boot_sector.bytes_per_sector;
    
    sector += record_num * sectors_per_record;
    
    // 读取MFT记录
    for (uint32_t i = 0; i < sectors_per_record; i++) {
        if (ata_read_sector(sector + i, (uint8_t*)buffer + i * fs->boot_sector.bytes_per_sector) != 0) {
            return -1;
        }
    }
    
    // 验证MFT记录签名
    mft_record_header_t* header = (mft_record_header_t*)buffer;
    if (header->signature[0] != 'F' || header->signature[1] != 'I' ||
        header->signature[2] != 'L' || header->signature[3] != 'E') {
        return -2; // 无效的MFT记录
    }
    
    return 0;
}

// 寻找文件
int ntfs_find_file(ntfs_fs_t* fs, const char* path, ntfs_file_t* file) {
    // 简化版实现：只支持根目录下的文件
    if (path[0] == '/' && path[1] != '\0') {
        // 读取根目录MFT记录(通常是记录5)
        uint8_t mft_record[1024];
        if (read_mft_record(fs, 5, mft_record) != 0) {
            return -1;
        }
        
        // TODO: 实现文件名查找逻辑
        // 这里需要解析MFT记录，查找索引，然后比较文件名
        
        return -1; // 简化版本：未找到文件
    }
    
    return -1; // 不支持的路径
}

// 读取文件内容
int ntfs_read_file(ntfs_fs_t* fs, ntfs_file_t* file, uint64_t offset, uint32_t size, void* buffer) {
    // 检查参数
    if (!fs->mounted || !file || !buffer) {
        return -1;
    }
    
    // 检查偏移量和大小
    if (offset >= file->size) {
        return 0; // 无数据可读
    }
    
    if (offset + size > file->size) {
        size = file->size - offset; // 调整读取大小
    }
    
    // TODO: 实现文件数据读取
    // 需要解析文件的DATA属性，处理运行列表，然后读取相应的扇区
    
    return -1; // 简化版本：未实现
}

// 写入文件内容
int ntfs_write_file(ntfs_fs_t* fs, ntfs_file_t* file, uint64_t offset, uint32_t size, const void* buffer) {
    // 检查参数
    if (!fs->mounted || !file || !buffer) {
        return -1;
    }
    
    // TODO: 实现文件数据写入
    // 需要处理文件扩展，分配新簇，更新运行列表等
    
    return -1; // 简化版本：未实现
}

// 创建文件
int ntfs_create_file(ntfs_fs_t* fs, const char* path, ntfs_file_t* file) {
    // 检查参数
    if (!fs->mounted || !path || !file) {
        return -1;
    }
    
    // TODO: 实现文件创建
    // 需要分配新的MFT记录，初始化属性，更新目录索引等
    
    return -1; // 简化版本：未实现
}

// 删除文件
int ntfs_delete_file(ntfs_fs_t* fs, const char* path) {
    // 检查参数
    if (!fs->mounted || !path) {
        return -1;
    }
    
    // TODO: 实现文件删除
    // 需要从目录索引中移除，释放数据簇，标记MFT记录为未使用等
    
    return -1; // 简化版本：未实现
}

// 列出目录内容
int ntfs_list_directory(ntfs_fs_t* fs, const char* path, ntfs_file_t** files, uint32_t* count) {
    // 检查参数
    if (!fs->mounted || !path || !files || !count) {
        return -1;
    }
    
    // TODO: 实现目录遍历
    // 需要解析目录的索引属性，遍历索引项
    
    *count = 0; // 简化版本：返回空目录
    return 0;
}

// 格式化NTFS分区
int ntfs_format(uint32_t disk_sector, uint64_t size) {
    // 添加一个外部可见的错误变量，帮助调试
    extern void print_string(const char* str);
    extern void print_int(int num);
    extern void print_newline();
    
    // 准备引导扇区
    memset(disk_buffer, 0, 512);
    
    // 设置基本字段
    ntfs_boot_sector_t* bs = (ntfs_boot_sector_t*)disk_buffer;
    
    // 跳转指令
    bs->jump[0] = 0xEB;
    bs->jump[1] = 0x52;
    bs->jump[2] = 0x90;
    
    // OEM ID "NTFS    "
    memcpy(bs->oem_id, "NTFS    ", 8);
    
    // 基本参数
    bs->bytes_per_sector = 512;
    bs->sectors_per_cluster = 8;
    bs->reserved_sectors = 0;
    bs->num_fats = 0;
    bs->root_entries = 0;
    bs->total_sectors_16 = 0;
    bs->media_type = 0xF8; // 固定磁盘
    bs->sectors_per_fat = 0;
    bs->sectors_per_track = 63;
    bs->num_heads = 255;
    bs->hidden_sectors = disk_sector;
    bs->total_sectors_32 = (size / 512) > 0xFFFFFFFF ? 0 : (uint32_t)(size / 512);
    
    // NTFS特有字段
    bs->sectors_per_fat_32 = 0;
    bs->mft_cluster = 4; // MFT起始簇
    bs->mft_mirror_cluster = size / (512 * 8) / 2; // MFT镜像在分区中间
    bs->clusters_per_mft_record = -10; // 1024字节每MFT记录
    bs->clusters_per_index_record = -10; // 1024字节每索引记录
    
    // 卷序列号
    bs->volume_serial = 0x12345678;
    
    // 调试输出
    print_string("正在写入NTFS引导扇区到LBA: ");
    print_int(disk_sector);
    print_newline();
    
    // 写入引导扇区
    int result = ata_write_sector(disk_sector, disk_buffer);
    if (result != 0) {
        print_string("NTFS引导扇区写入失败，错误代码: ");
        print_int(result);
        print_newline();
        return result;
    }
    
    print_string("NTFS引导扇区写入成功，正在初始化文件系统...");
    print_newline();
    
    // TODO: 创建MFT和其他系统文件
    // 需要创建$MFT, $MFTMirr, $LogFile, $Volume, $AttrDef, 
    // $Root, $Bitmap, $Boot, $BadClus, $Secure, $UpCase, $Extend等系统文件
    
    // 因为我们还没实现完整的NTFS初始化，所以这里创建一个简化的MFT
    // 留到未来实现
    print_string("格式化成功，但系统文件未完全创建（简化版本）");
    print_newline();
    
    return 0; // 简化版本：仅写入引导扇区
} 