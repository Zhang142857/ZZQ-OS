// 添加版本号定义
#define VERSION "V1.0"

#include <stdint.h>
#include <stddef.h>  // 添加这个，为了NULL定义
#include "memory.h"
#include "timer.h"
#include "string.h"
#include "zfs.h" // 添加ZFS支持

// 声明get_ntfs_fs函数 - 必须在第一次使用前声明，避免隐式声明
extern zfs_fs_t* get_zfs_fs(void);

#define VIDEO_MEMORY 0xb8000
#define ROWS 25
#define COLS 80
#define WHITE_ON_BLACK 0x07
#define GREEN_ON_BLACK 0x0A
#define RED_ON_BLACK 0x0C
#define BLUE_ON_BLACK 0x01

// 输入缓冲区
#define INPUT_BUFFER_SIZE 256
char input_buffer[INPUT_BUFFER_SIZE];
int input_position = 0;
int input_ready = 0;

// 光标位置
int cursor_x = 0;
int cursor_y = 0;

// 文本颜色
uint8_t text_color = WHITE_ON_BLACK;

// 当前目录
char current_dir[256] = "/";

// 命令历史
#define HISTORY_SIZE 10
char command_history[HISTORY_SIZE][INPUT_BUFFER_SIZE];
int history_count = 0;
int history_index = -1;

// 函数声明
void update_cursor();
void print_string(const char* str);
void print_char(char c);
void print_newline();
void int_to_string(int num, char* str);
void print_int(int num);

// 端口输入/输出函数
unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__("in %%dx, %%al" : "=a" (result) : "d" (port));
    return result;
}

void outb(unsigned short port, unsigned char data) {
    __asm__("out %%al, %%dx" : : "a" (data), "d" (port));
}

// 键盘端口
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

// 设置文本颜色
void set_text_color(uint8_t color) {
    text_color = color;
}

// 清屏
void clear_screen() {
    char* video_memory = (char*)VIDEO_MEMORY;
    
    for(int i = 0; i < ROWS * COLS; i++) {
        video_memory[i*2] = ' ';
        video_memory[i*2+1] = text_color;
    }
    
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

// 更新光标位置
void update_cursor() {
    unsigned short position = cursor_y * COLS + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
    
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

// 打印字符
void print_char(char c) {
    char* video_memory = (char*)VIDEO_MEMORY;
    
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {  // 退格
        if (cursor_x > 0) {
            cursor_x--;
            // 清除当前位置的字符
            video_memory[(cursor_y * COLS + cursor_x) * 2] = ' ';
            video_memory[(cursor_y * COLS + cursor_x) * 2 + 1] = text_color;
        }
    } else {
        video_memory[(cursor_y * COLS + cursor_x) * 2] = c;
        video_memory[(cursor_y * COLS + cursor_x) * 2 + 1] = text_color;
        cursor_x++;
    }
    
    if (cursor_x >= COLS) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= ROWS) {
        // 移动所有行向上一行
        for(int i = 0; i < (ROWS-1) * COLS; i++) {
            video_memory[i*2] = video_memory[(i + COLS) * 2];
            video_memory[i*2+1] = video_memory[(i + COLS) * 2 + 1];
        }
        
        // 清除最后一行
        for(int i = (ROWS-1) * COLS; i < ROWS * COLS; i++) {
            video_memory[i*2] = ' ';
            video_memory[i*2+1] = text_color;
        }
        
        cursor_y = ROWS - 1;
    }
    
    update_cursor();
}

// 打印字符串
void print_string(const char* str) {
    int i = 0;
    while(str[i] != '\0') {
        print_char(str[i]);
        i++;
    }
}

// 打印新行
void print_newline() {
    print_char('\n');
}

// 打印数字
void print_int(int num) {
    char buffer[32];
    int_to_string(num, buffer);
    print_string(buffer);
}

// 整数转字符串
void int_to_string(int num, char* str) {
    int i = 0;
    int sign = 0;
    
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    if (num < 0) {
        sign = 1;
        num = -num;
    }
    
    do {
        str[i++] = num % 10 + '0';
        num /= 10;
    } while (num > 0);
    
    if (sign) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    // 反转字符串
    int j, k;
    for (j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

// 读取键盘按键
char get_key() {
    // 检查键盘是否有数据
    if((inb(KEYBOARD_STATUS_PORT) & 1) == 0) {
        return 0;
    }
    
    // 读取键盘数据
    unsigned char keycode = inb(KEYBOARD_DATA_PORT);
    
    // 键盘扫描码到ASCII码的基本映射
    static const char keymap[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };
    
    // 如果是按键释放事件（高位为1），忽略
    if(keycode & 0x80) {
        return 0;
    }
    
    // 将扫描码转换为ASCII
    if(keycode < sizeof(keymap)) {
        return keymap[keycode];
    }
    
    return 0;
}

// 打印提示符
void print_prompt() {
    set_text_color(GREEN_ON_BLACK);
    print_string("ZZQ> ");
    set_text_color(WHITE_ON_BLACK);
}

// 处理键盘输入
void handle_keyboard() {
    char key = get_key();
    
    // 如果没有按键，立即返回
    if(key == 0) {
        return;
    }
    
    // 回车键 - 完成命令输入
    if(key == '\n') {
        // 添加到命令历史
        if(input_position > 0) {
            if(history_count < HISTORY_SIZE) {
                strcpy(command_history[history_count++], input_buffer);
            } else {
                // 移动历史命令，丢弃最旧的
                for(int i = 0; i < HISTORY_SIZE - 1; i++) {
                    strcpy(command_history[i], command_history[i + 1]);
                }
                strcpy(command_history[HISTORY_SIZE - 1], input_buffer);
            }
            history_index = -1;
        }
        
        input_buffer[input_position] = '\0';
        print_char('\n');
        input_ready = 1;
    }
    // 退格键 - 删除字符
    else if(key == '\b') {
        if(input_position > 0) {
            input_position--;
            print_char('\b');
        }
    }
    // 普通字符 - 添加到缓冲区
    else if(key >= ' ' && key <= '~' && input_position < INPUT_BUFFER_SIZE - 1) {
        input_buffer[input_position++] = key;
        print_char(key);
    }
}

// 读取用户输入
void read_input() {
    input_position = 0;
    input_ready = 0;
    
    while(!input_ready) {
        handle_keyboard();
        
        // 简单的CPU降低使用率
        for(int i = 0; i < 1000000; i++) {
            __asm__("nop");
        }
    }
}

// 文件系统相关定义
#define MAX_FILES 32
#define MAX_FILENAME_LENGTH 32
#define MAX_FILE_SIZE 4096

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    uint32_t size;
    uint8_t data[MAX_FILE_SIZE];
    uint8_t in_use;
} file_t;

file_t file_table[MAX_FILES];
int fs_initialized = 0;

// 初始化文件系统
void init_filesystem() {
    if(!fs_initialized) {
        for(int i = 0; i < MAX_FILES; i++) {
            file_table[i].in_use = 0;
        }
        fs_initialized = 1;
    }
}

// 查找文件
int find_file(const char* filename) {
    for(int i = 0; i < MAX_FILES; i++) {
        if(file_table[i].in_use && strcmp(file_table[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1;
}

// 创建文件
int create_file(const char* filename) {
    // 检查文件是否已存在
    if(find_file(filename) >= 0) {
        return -1; // 文件已存在
    }
    
    // 查找空闲槽位
    int index = -1;
    for(int i = 0; i < MAX_FILES; i++) {
        if(!file_table[i].in_use) {
            index = i;
            break;
        }
    }
    
    if(index == -1) {
        return -2; // 文件表已满
    }
    
    // 初始化文件
    strcpy(file_table[index].filename, filename);
    file_table[index].size = 0;
    file_table[index].in_use = 1;
    
    return index;
}

// 写入文件
int write_file(const char* filename, const char* data, uint32_t size) {
    int index = find_file(filename);
    if(index < 0) {
        index = create_file(filename);
        if(index < 0) {
            return -1; // 无法创建文件
        }
    }
    
    // 检查文件大小限制
    if(size > MAX_FILE_SIZE) {
        return -2; // 文件太大
    }
    
    // 写入数据
    memcpy(file_table[index].data, data, size);
    file_table[index].size = size;
    
    return 0;
}

// 读取文件
int read_file(const char* filename, char* buffer, uint32_t max_size) {
    int index = find_file(filename);
    if(index < 0) {
        return -1; // 文件不存在
    }
    
    uint32_t size = file_table[index].size;
    if(size > max_size) {
        size = max_size;
    }
    
    memcpy(buffer, file_table[index].data, size);
    return size;
}

// 删除文件
int delete_file(const char* filename) {
    int index = find_file(filename);
    if(index < 0) {
        return -1; // 文件不存在
    }
    
    file_table[index].in_use = 0;
    return 0;
}

// 列出文件
void list_files() {
    int found = 0;
    
    for(int i = 0; i < MAX_FILES; i++) {
        if(file_table[i].in_use) {
            found = 1;
            print_string(file_table[i].filename);
            print_string(" (");
            print_int(file_table[i].size);
            print_string(" bytes)\n");
        }
    }
    
    if(!found) {
        print_string("No files found.\n");
    }
}

// 保存文件系统到磁盘（模拟）
void sync_filesystem() {
    // 这里只是模拟，实际实现会涉及到ATA驱动和文件系统
    print_string("Filesystem synchronized to disk (simulated).\n");
}

// 命令: 帮助
void cmd_help() {
    set_text_color(BLUE_ON_BLACK);
    print_string("可用命令:\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("  help      - 显示帮助信息\n");
    print_string("  clear     - 清屏\n");
    print_string("  info      - 显示系统信息\n");
    print_string("  about     - 关于ZZQ OS及其创建者\n");
    print_string("  memory    - 显示内存统计信息\n");
    print_string("  memtest   - 运行内存分配测试\n");
    print_string("  clock     - 显示实时时钟\n");
    print_string("  ticks     - 显示系统时钟计数\n");
    print_string("  shutdown  - 关闭系统 (QEMU环境)\n");
    print_string("  restart   - 重启系统 (QEMU环境)\n");
    print_string("  sync      - 将文件系统数据刷新到磁盘\n");
    
    set_text_color(BLUE_ON_BLACK);
    print_string("文件系统命令（内存中）:\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("  format    - 格式化文件系统\n");
    print_string("  mount     - 挂载文件系统\n");
    print_string("  ls        - 列出文件\n");
    print_string("  touch     - 创建空文件\n");
    print_string("  cat       - 显示文件内容\n");
    print_string("  rm        - 删除文件\n");
    print_string("  nano      - 简易文本编辑器\n");
    
    set_text_color(BLUE_ON_BLACK);
    print_string("ZFS文件系统命令（持久存储）:\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("  zfs_format   - 格式化ZFS分区\n");
    print_string("  zfs_mount    - 挂载ZFS文件系统\n");
    print_string("  zfs_unmount  - 卸载ZFS文件系统\n");
    print_string("  zfs_ls       - 列出ZFS目录内容\n");
    print_string("  zfs_cat      - 显示ZFS文件内容\n");
    print_string("  zfs_write    - 写入ZFS文件\n");
    print_string("  zfs_rm       - 删除ZFS文件\n");
}

// 命令: 关于
void cmd_about() {
    set_text_color(GREEN_ON_BLACK);
    print_string("About ZZQ OS ");
    print_string(VERSION);
    print_string("\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("ZZQ OS is a simple operating system for learning purposes.\n");
    print_string("It features a basic command-line interface, memory management,\n");
    print_string("and a simple in-memory filesystem.\n\n");
    print_string("Created as a learning and demonstration platform.\n");
    print_string("Copyright (c) 2023-2024\n");
}

// 命令: 系统信息
void cmd_info() {
    set_text_color(GREEN_ON_BLACK);
    print_string("System Information\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("OS Name: ZZQ OS\n");
    print_string("Version: ");
    print_string(VERSION);
    print_string("\n");
    print_string("Architecture: x86 (32-bit)\n");
    print_string("Memory: 16 MB (simulated)\n");
    print_string("Filesystem: In-memory (RAM)\n");
}

// 命令: 显示时钟计数
void cmd_ticks() {
    print_string("System uptime: ");
    print_int(get_tick());
    print_string(" ticks\n");
}

// 简易文件创建命令
void cmd_touch(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: touch <filename>\n");
        return;
    }
    
    int result = create_file(filename);
    if(result >= 0) {
        print_string("File created: ");
        print_string(filename);
        print_newline();
    } else if(result == -1) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: File already exists\n");
        set_text_color(WHITE_ON_BLACK);
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("Error: File system full\n");
        set_text_color(WHITE_ON_BLACK);
    }
}

// 查看文件内容命令
void cmd_cat(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: cat <filename>\n");
        return;
    }
    
    char buffer[MAX_FILE_SIZE + 1];
    int size = read_file(filename, buffer, MAX_FILE_SIZE);
    
    if(size >= 0) {
        buffer[size] = '\0';
        print_string(buffer);
        print_newline();
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("Error: File not found\n");
        set_text_color(WHITE_ON_BLACK);
    }
}

// 删除文件命令
void cmd_rm(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: rm <filename>\n");
        return;
    }
    
    int result = delete_file(filename);
    if(result == 0) {
        print_string("File deleted: ");
        print_string(filename);
        print_newline();
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("Error: File not found\n");
        set_text_color(WHITE_ON_BLACK);
    }
}

// 简易文本编辑器命令
void cmd_nano(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: nano <filename>\n");
        return;
    }
    
    char content[MAX_FILE_SIZE + 1] = {0};
    
    // 如果文件存在，先读取内容
    int index = find_file(filename);
    if(index >= 0) {
        read_file(filename, content, MAX_FILE_SIZE);
    }
    
    // 编辑提示
    clear_screen();
    set_text_color(BLUE_ON_BLACK);
    print_string("Editing file: ");
    print_string(filename);
    print_string(" (Max ");
    print_int(MAX_FILE_SIZE);
    print_string(" bytes)\n");
    print_string("Type your content, press ENTER to save and exit:\n");
    set_text_color(WHITE_ON_BLACK);
    
    if(index >= 0) {
        print_string(content);
    }
    
    // 读取内容
    read_input();
    
    // 保存文件
    write_file(filename, input_buffer, input_position);
    
    clear_screen();
    print_string("File saved: ");
    print_string(filename);
    print_newline();
}

// 文件系统格式化命令
void cmd_format() {
    set_text_color(RED_ON_BLACK);
    print_string("Warning: This will erase all files. Proceed? (y/n): ");
    set_text_color(WHITE_ON_BLACK);
    
    read_input();
    
    if(input_buffer[0] == 'y' || input_buffer[0] == 'Y') {
        // 重新初始化文件系统
        for(int i = 0; i < MAX_FILES; i++) {
            file_table[i].in_use = 0;
        }
        
        print_string("Filesystem formatted successfully.\n");
        sync_filesystem();
    } else {
        print_string("Format operation canceled.\n");
    }
}

// 挂载文件系统命令
void cmd_mount() {
    print_string("Mounting filesystem...\n");
    init_filesystem();
    print_string("Filesystem mounted successfully.\n");
}

// ZFS文件系统命令
void cmd_zfs_format() {
    set_text_color(RED_ON_BLACK);
    print_string("警告: 这将格式化ZFS分区并删除所有数据！继续? (y/n): ");
    set_text_color(WHITE_ON_BLACK);
    
    read_input();
    
    if(input_buffer[0] == 'y' || input_buffer[0] == 'Y') {
        print_string("格式化ZFS分区中，请稍候...\n");
        
        // 使用ZFS格式化函数格式化分区
        // 假设磁盘分区从LBA 2048开始，大小为100MB
        int result = zfs_format(2048, 100 * 1024 * 1024);
        
        if(result == 0) {
            print_string("ZFS分区格式化成功！\n");
        } else {
            set_text_color(RED_ON_BLACK);
            print_string("ZFS分区格式化失败，错误代码: ");
            print_int(result);
            print_newline();
            set_text_color(WHITE_ON_BLACK);
        }
    } else {
        print_string("格式化操作已取消。\n");
    }
}

void cmd_zfs_mount() {
    print_string("挂载ZFS文件系统...\n");
    
    // 获取ZFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 初始化ZFS文件系统
    int result = ntfs_init(fs, 2048); // 从LBA 2048开始
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("初始化ZFS文件系统失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 挂载文件系统
    result = zfs_mount(fs);
    
    if(result == 0) {
        print_string("ZFS文件系统挂载成功！\n");
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("ZFS文件系统挂载失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

void cmd_zfs_unmount() {
    print_string("卸载ZFS文件系统...\n");
    
    // 获取ZFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 卸载文件系统
    int result = zfs_unmount(fs);
    
    if(result == 0) {
        print_string("ZFS文件系统已卸载。\n");
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("ZFS文件系统卸载失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

void cmd_zfs_ls(const char* path) {
    // 获取ZFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 检查是否已挂载
    if(!fs->mounted) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: ZFS文件系统未挂载\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 如果没有指定路径，使用根目录
    if(strlen(path) == 0) {
        path = "/";
    }
    
    print_string("ZFS目录列表: ");
    print_string(path);
    print_newline();
    
    // 列出目录内容
    ntfs_file_t* files = NULL;
    uint32_t count = 0;
    
    int result = ntfs_list_directory(fs, path, &files, &count);
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("列出目录失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    if(count == 0) {
        print_string("目录为空。\n");
        return;
    }
    
    // 显示文件列表
    for(uint32_t i = 0; i < count; i++) {
        // 显示文件属性
        if(files[i].attributes & 0x10) { // 目录
            set_text_color(BLUE_ON_BLACK);
            print_string("[DIR] ");
        } else {
            set_text_color(WHITE_ON_BLACK);
            print_string("[FILE] ");
        }
        
        // 显示文件名
        print_string(files[i].name);
        
        // 如果是文件，显示大小
        if(!(files[i].attributes & 0x10)) {
            print_string(" (");
            print_int(files[i].size);
            print_string(" bytes)");
        }
        
        print_newline();
    }
    
    set_text_color(WHITE_ON_BLACK);
}

void cmd_zfs_cat(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: 缺少文件名\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("用法: zfs_cat <文件名>\n");
        return;
    }
    
    // 获取ZFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 检查是否已挂载
    if(!fs->mounted) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: ZFS文件系统未挂载\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 查找文件
    ntfs_file_t file;
    int result = ntfs_find_file(fs, filename, &file);
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: 文件未找到\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 分配缓冲区
    char* buffer = (char*)malloc(file.size + 1);
    if(!buffer) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: 内存分配失败\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 读取文件内容
    result = ntfs_read_file(fs, &file, 0, file.size, buffer);
    
    if(result < 0) {
        set_text_color(RED_ON_BLACK);
        print_string("读取文件失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        free(buffer);
        return;
    }
    
    // 确保字符串结束
    buffer[file.size] = '\0';
    
    // 显示文件内容
    print_string(buffer);
    print_newline();
    
    // 释放缓冲区
    free(buffer);
}

void cmd_zfs_write(const char* args) {
    char filename[MAX_FILENAME_LENGTH] = {0};
    
    // 解析文件名
    int i = 0;
    while(args[i] != ' ' && args[i] != '\0' && i < MAX_FILENAME_LENGTH - 1) {
        filename[i] = args[i];
        i++;
    }
    filename[i] = '\0';
    
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: 缺少文件名\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("用法: zfs_write <文件名> <内容>\n");
        return;
    }
    
    // 跳过空格
    while(args[i] == ' ') {
        i++;
    }
    
    // 获取内容
    const char* content = &args[i];
    
    // 获取ZFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 检查是否已挂载
    if(!fs->mounted) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: ZFS文件系统未挂载\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 查找或创建文件
    ntfs_file_t file;
    if(ntfs_find_file(fs, filename, &file) != 0) {
        // 文件不存在，创建新文件
        int result = ntfs_create_file(fs, filename, &file);
        if(result != 0) {
            set_text_color(RED_ON_BLACK);
            print_string("创建文件失败，错误代码: ");
            print_int(result);
            print_newline();
            set_text_color(WHITE_ON_BLACK);
            return;
        }
    }
    
    // 写入内容
    int content_length = strlen(content);
    int result = zfs_write_file(fs, &file, 0, content_length, content);
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("写入文件失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    print_string("文件写入成功: ");
    print_string(filename);
    print_string(" (");
    print_int(content_length);
    print_string(" bytes)\n");
}

void cmd_zfs_rm(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: 缺少文件名\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("用法: zfs_rm <文件名>\n");
        return;
    }
    
    // 获取ZFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 检查是否已挂载
    if(!fs->mounted) {
        set_text_color(RED_ON_BLACK);
        print_string("错误: ZFS文件系统未挂载\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 删除文件
    int result = ntfs_delete_file(fs, filename);
    
    if(result == 0) {
        print_string("文件已删除: ");
        print_string(filename);
        print_newline();
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("删除文件失败，错误代码: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

// 解析输入命令
void parse_command(char* command) {
    // 跳过开头的空格
    while(*command == ' ') {
        command++;
    }
    
    // 提取第一个词作为命令名
    char cmd_name[32] = {0};
    char args[INPUT_BUFFER_SIZE] = {0};
    
    int i = 0;
    while(command[i] != ' ' && command[i] != '\0' && i < 31) {
        cmd_name[i] = command[i];
        i++;
    }
    cmd_name[i] = '\0';
    
    // 如果有参数，提取参数
    if(command[i] != '\0') {
        int j = 0;
        i++; // 跳过空格
        
        // 跳过多余的空格
        while(command[i] == ' ') {
            i++;
        }
        
        while(command[i] != '\0') {
            args[j++] = command[i++];
        }
        args[j] = '\0';
    }
    
    // 执行对应的命令
    if(strcmp(cmd_name, "help") == 0) {
        cmd_help();
    } else if(strcmp(cmd_name, "clear") == 0) {
        clear_screen();
    } else if(strcmp(cmd_name, "info") == 0) {
        cmd_info();
    } else if(strcmp(cmd_name, "about") == 0) {
        cmd_about();
    } else if(strcmp(cmd_name, "memory") == 0) {
        memory_stats();
    } else if(strcmp(cmd_name, "ticks") == 0) {
        cmd_ticks();
    } else if(strcmp(cmd_name, "ls") == 0) {
        list_files();
    } else if(strcmp(cmd_name, "touch") == 0) {
        cmd_touch(args);
    } else if(strcmp(cmd_name, "cat") == 0) {
        cmd_cat(args);
    } else if(strcmp(cmd_name, "rm") == 0) {
        cmd_rm(args);
    } else if(strcmp(cmd_name, "nano") == 0) {
        cmd_nano(args);
    } else if(strcmp(cmd_name, "format") == 0) {
        cmd_format();
    } else if(strcmp(cmd_name, "mount") == 0) {
        cmd_mount();
    } else if(strcmp(cmd_name, "sync") == 0) {
        sync_filesystem();
    } 
    // ZFS相关命令
    else if(strcmp(cmd_name, "zfs_format") == 0) {
        cmd_zfs_format();
    } else if(strcmp(cmd_name, "zfs_mount") == 0) {
        cmd_zfs_mount();
    } else if(strcmp(cmd_name, "zfs_unmount") == 0) {
        cmd_zfs_unmount();
    } else if(strcmp(cmd_name, "zfs_ls") == 0) {
        cmd_zfs_ls(args);
    } else if(strcmp(cmd_name, "zfs_cat") == 0) {
        cmd_zfs_cat(args);
    } else if(strcmp(cmd_name, "zfs_write") == 0) {
        cmd_zfs_write(args);
    } else if(strcmp(cmd_name, "zfs_rm") == 0) {
        cmd_zfs_rm(args);
    } else if(cmd_name[0] == '\0') {
        // 空命令，不做任何事
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("未知命令: ");
        print_string(cmd_name);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        print_string("输入 'help' 获取可用命令列表。\n");
    }
}

// 主函数
void kmain(void) {
    // 初始化内存管理系统
    memory_init();
    
    // 初始化定时器（100Hz）
    init_timer(100);
    
    // 初始化文件系统
    init_filesystem();
    
    // 清屏并显示欢迎信息
    clear_screen();
    set_text_color(WHITE_ON_BLACK);
    print_string("Welcome to ZZQ OS VERSION ");
    print_string(VERSION);
    print_string("!\n");
    print_string("\nSimplified OS for learning and VMware demonstration\n");
    print_string("Copyright (c) 2023-2024\n\n");
    
    // 主循环
    while(1) {
        print_prompt();
        read_input();
        parse_command(input_buffer);
    }
} 
// 显示ZFS文件系统信息
void cmd_zfs_info() {
    zfs_fs_t* fs = get_zfs_fs();
    zfs_dump_info(fs);
}
