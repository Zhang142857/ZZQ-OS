#include "zfs.h"
#include "string.h"

// 外部声明
extern void clear_screen(void);
extern void print_string(const char* str);
extern void print_int(int num);
extern void print_newline(void);
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

// 常量
#define VIDEO_MEMORY 0xB8000
#define KEYBOARD_DATA_PORT 0x60
#define MAX_COMMAND_LENGTH 256
#define MAX_ARGS 10

// 全局变量
static char command_buffer[MAX_COMMAND_LENGTH];
static int command_pos = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int text_color = 0x0F; // 白色文字
static char current_directory[256] = "/";

// 工具函数
static uint64_t get_current_time(void) {
    // 简化版：返回自启动以来的时钟计时
    static uint64_t ticks = 0;
    return ticks++;
}

// 控制台输出函数
static void set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    
    // 计算光标位置
    unsigned short position = (y * 80) + x;
    
    // 通过端口命令设置光标位置
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

static void update_cursor(void) {
    set_cursor(cursor_x, cursor_y);
}

static void print_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            // 清除字符
            unsigned char* video_memory = (unsigned char*)VIDEO_MEMORY;
            int offset = (cursor_y * 80 + cursor_x) * 2;
            video_memory[offset] = ' ';
            video_memory[offset + 1] = text_color;
        }
    } else {
        // 正常字符输出
        unsigned char* video_memory = (unsigned char*)VIDEO_MEMORY;
        int offset = (cursor_y * 80 + cursor_x) * 2;
        video_memory[offset] = c;
        video_memory[offset + 1] = text_color;
        cursor_x++;
    }
    
    // 处理行末换行
    if (cursor_x >= 80) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // 屏幕滚动
    if (cursor_y >= 25) {
        // 滚动屏幕
        unsigned char* video_memory = (unsigned char*)VIDEO_MEMORY;
        for (int i = 0; i < 24 * 80 * 2; i++) {
            video_memory[i] = video_memory[i + 80 * 2];
        }
        
        // 清除最后一行
        for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i + 1] = text_color;
        }
        
        cursor_y = 24;
    }
    
    update_cursor();
}

// 命令处理
static void parse_command(char* command, char** args, int* argc) {
    char* token = command;
    *argc = 0;
    
    // 跳过前导空格
    while (*token == ' ') {
        token++;
    }
    
    if (*token == '\0') {
        return; // 空命令
    }
    
    args[(*argc)++] = token;
    
    // 解析参数
    while (*token) {
        if (*token == ' ') {
            *token = '\0'; // 截断参数
            token++;
            
            // 跳过连续空格
            while (*token == ' ') {
                token++;
            }
            
            if (*token == '\0') {
                break; // 命令结束
            }
            
            if (*argc < MAX_ARGS) {
                args[(*argc)++] = token;
            } else {
                break; // 参数太多
            }
        } else {
            token++;
        }
    }
}

// 命令处理函数
static void cmd_help(int argc, char** args) {
    print_string("ZZQ-OS v1.0 with ZFS文件系统\n");
    print_string("可用命令:\n");
    print_string("  help                  - 显示此帮助\n");
    print_string("  clear                 - 清屏\n");
    print_string("  zfs_format <扇区> <大小> [卷标] - 格式化ZFS文件系统\n");
    print_string("  zfs_mount             - 挂载ZFS文件系统\n");
    print_string("  zfs_unmount           - 卸载ZFS文件系统\n");
    print_string("  ls                    - 列出目录内容\n");
    print_string("  cat <文件名>           - 显示文件内容\n");
    print_string("  write <文件名> <内容>   - 写入文件\n");
    print_string("  mkdir <目录名>         - 创建目录\n");
    print_string("  rm <文件/目录名>        - 删除文件或目录\n");
}

static void cmd_clear(int argc, char** args) {
    clear_screen();
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

static void cmd_zfs_format(int argc, char** args) {
    if (argc < 3) {
        print_string("用法: zfs_format <扇区> <大小> [卷标]\n");
        return;
    }
    
    uint32_t sector = atoi(args[1]);
    uint64_t size = atoi(args[2]);
    const char* label = (argc > 3) ? args[3] : "ZZQ-ZFS";
    
    print_string("正在格式化 ZFS...\n");
    int result = zfs_format(sector, size * 1024 * 1024, label);
    
    if (result == 0) {
        print_string("格式化成功！\n");
    } else {
        print_string("格式化失败，错误代码: ");
        print_int(result);
        print_newline();
    }
}

static void cmd_zfs_mount(int argc, char** args) {
    zfs_fs_t* fs = get_zfs_fs();
    
    if (fs->mounted) {
        print_string("文件系统已挂载\n");
        return;
    }
    
    // 默认从扇区100开始
    if (zfs_init(fs, 100) != 0) {
        print_string("初始化ZFS失败，请先格式化\n");
        return;
    }
    
    if (zfs_mount(fs) != 0) {
        print_string("挂载失败\n");
        return;
    }
    
    print_string("ZFS文件系统已成功挂载\n");
    print_string("卷标: ");
    print_string((const char*)fs->superblock.label);
    print_newline();
    print_string("总块数: ");
    print_int(fs->superblock.total_blocks);
    print_newline();
    print_string("空闲块数: ");
    print_int(fs->superblock.free_blocks);
    print_newline();
}

static void cmd_zfs_unmount(int argc, char** args) {
    zfs_fs_t* fs = get_zfs_fs();
    
    if (!fs->mounted) {
        print_string("文件系统未挂载\n");
        return;
    }
    
    if (zfs_unmount(fs) != 0) {
        print_string("卸载失败\n");
        return;
    }
    
    print_string("ZFS文件系统已成功卸载\n");
}

// 主函数
void kernel_main(void) {
    char* args[MAX_ARGS];
    int argc;
    unsigned char key_code;
    
    // 清屏
    clear_screen();
    
    // 打印欢迎信息
    print_string("欢迎使用 ZZQ-OS v1.0 带有ZFS文件系统\n");
    print_string("输入 'help' 获取可用命令\n");
    
    // 命令行循环
    while (1) {
        // 显示提示符
        print_string("zzq-os> ");
        
        // 重置命令缓冲区
        command_pos = 0;
        memset(command_buffer, 0, MAX_COMMAND_LENGTH);
        
        // 读取命令
        while (1) {
            // 等待按键
            while ((inb(0x64) & 1) == 0);
            
            // 读取按键
            key_code = inb(KEYBOARD_DATA_PORT);
            
            // 只处理按下事件
            if (key_code & 0x80) {
                continue;
            }
            
            // 映射扫描码到ASCII
            char c = 0;
            switch (key_code) {
                case 0x1C: // 回车
                    print_char('\n');
                    goto process_command;
                case 0x0E: // 退格
                    if (command_pos > 0) {
                        command_pos--;
                        command_buffer[command_pos] = '\0';
                        print_char('\b');
                    }
                    break;
                case 0x1D: // Ctrl
                case 0x2A: // Shift (左)
                case 0x36: // Shift (右)
                case 0x38: // Alt
                    break;
                case 0x39: // 空格
                    c = ' ';
                    break;
                default:
                    // 简化的键盘映射
                    if (key_code >= 0x02 && key_code <= 0x0D) { // 1-=
                        static const char keys[] = "1234567890-=";
                        c = keys[key_code - 0x02];
                    } else if (key_code >= 0x10 && key_code <= 0x1B) { // qwertyuiop[]
                        static const char keys[] = "qwertyuiop[]";
                        c = keys[key_code - 0x10];
                    } else if (key_code >= 0x1E && key_code <= 0x28) { // asdfghjkl;'
                        static const char keys[] = "asdfghjkl;'";
                        c = keys[key_code - 0x1E];
                    } else if (key_code >= 0x2C && key_code <= 0x35) { // zxcvbnm,./
                        static const char keys[] = "zxcvbnm,./";
                        c = keys[key_code - 0x2C];
                    }
                    break;
            }
            
            if (c != 0 && command_pos < MAX_COMMAND_LENGTH - 1) {
                command_buffer[command_pos++] = c;
                print_char(c);
            }
        }
        
    process_command:
        // 确保命令以空字符结尾
        command_buffer[command_pos] = '\0';
        
        // 解析命令
        parse_command(command_buffer, args, &argc);
        
        // 执行命令
        if (argc > 0) {
            if (strcmp(args[0], "help") == 0) {
                cmd_help(argc, args);
            } else if (strcmp(args[0], "clear") == 0) {
                cmd_clear(argc, args);
            } else if (strcmp(args[0], "zfs_format") == 0) {
                cmd_zfs_format(argc, args);
            } else if (strcmp(args[0], "zfs_mount") == 0) {
                cmd_zfs_mount(argc, args);
            } else if (strcmp(args[0], "zfs_unmount") == 0) {
                cmd_zfs_unmount(argc, args);
            } else {
                print_string("未知命令: ");
                print_string(args[0]);
                print_newline();
            }
        }
    }
}

// 辅助函数：字符串转整数
int atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    // 跳过空格
    while (*str == ' ') {
        str++;
    }
    
    // 处理符号
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // 转换数字
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
} 