#include "zfs.h"
#include "custom_string.h"

// 外部函数声明
extern void print_string(const char* str);
extern void print_int(int num);
extern void print_char(char c);
extern void print_newline(void);

// 简化的内核入口函数
void kmain() {
    print_string("ZZQ OS V1.0 - ZFS文件系统启动中...");
    print_newline();
    
    // 初始化ZFS文件系统
    zfs_fs_t* fs = get_zfs_fs();
    zfs_init(fs, 100); // 从第100个扇区开始
    
    // 格式化ZFS文件系统(假设大小为4MB)
    zfs_format(100, 4 * 1024 * 1024);
    
    // 挂载ZFS文件系统
    zfs_mount(fs);
    
    // 显示文件系统信息
    zfs_dump_info(fs);
    
    print_string("ZFS文件系统初始化完成！");
    print_newline();
    
    while(1) {
        // 简单的死循环保持系统运行
    }
}
