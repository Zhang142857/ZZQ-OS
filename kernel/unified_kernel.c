// 添加版本号定义
#define VERSION "V2.1"

#include <stdint.h>
#include <stddef.h>  // 添加这个，为了NULL定义
#include "memory.h"
#include "timer.h"
#include "string.h"
#include "ntfs.h" // NTFS支持

// 声明get_ntfs_fs函数
extern ntfs_fs_t* get_ntfs_fs(void);

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

// NTFS文件系统状态
int ntfs_initialized = 0;

// 键盘状态
int shift_pressed = 0;
int caps_lock_on = 0;

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
#define KEYBOARD_COMMAND_PORT 0x64

// 键盘LED
#define KEYBOARD_LED_SCROLL_LOCK 1
#define KEYBOARD_LED_NUM_LOCK 2
#define KEYBOARD_LED_CAPS_LOCK 4

// 键盘扫描码
#define KEY_ESC             0x01
#define KEY_1               0x02
#define KEY_2               0x03
#define KEY_3               0x04
#define KEY_4               0x05
#define KEY_5               0x06
#define KEY_6               0x07
#define KEY_7               0x08
#define KEY_8               0x09
#define KEY_9               0x0A
#define KEY_0               0x0B
#define KEY_MINUS           0x0C
#define KEY_EQUALS          0x0D
#define KEY_BACKSPACE       0x0E
#define KEY_TAB             0x0F
#define KEY_Q               0x10
#define KEY_W               0x11
#define KEY_E               0x12
#define KEY_R               0x13
#define KEY_T               0x14
#define KEY_Y               0x15
#define KEY_U               0x16
#define KEY_I               0x17
#define KEY_O               0x18
#define KEY_P               0x19
#define KEY_LEFTBRACKET     0x1A
#define KEY_RIGHTBRACKET    0x1B
#define KEY_ENTER           0x1C
#define KEY_LCTRL           0x1D
#define KEY_A               0x1E
#define KEY_S               0x1F
#define KEY_D               0x20
#define KEY_F               0x21
#define KEY_G               0x22
#define KEY_H               0x23
#define KEY_J               0x24
#define KEY_K               0x25
#define KEY_L               0x26
#define KEY_SEMICOLON       0x27
#define KEY_APOSTROPHE      0x28
#define KEY_BACKTICK        0x29
#define KEY_LSHIFT          0x2A
#define KEY_BACKSLASH       0x2B
#define KEY_Z               0x2C
#define KEY_X               0x2D
#define KEY_C               0x2E
#define KEY_V               0x2F
#define KEY_B               0x30
#define KEY_N               0x31
#define KEY_M               0x32
#define KEY_COMMA           0x33
#define KEY_PERIOD          0x34
#define KEY_SLASH           0x35
#define KEY_RSHIFT          0x36
#define KEY_MULTIPLY        0x37
#define KEY_LALT            0x38
#define KEY_SPACE           0x39
#define KEY_CAPSLOCK        0x3A

// 更新键盘LED状态
void update_keyboard_leds() {
    uint8_t led_status = 0;
    
    if (caps_lock_on) {
        led_status |= KEYBOARD_LED_CAPS_LOCK;
    }
    
    // 发送命令到键盘控制器
    outb(KEYBOARD_COMMAND_PORT, 0xED);
    // 等待键盘控制器准备好接收数据
    while ((inb(KEYBOARD_STATUS_PORT) & 2) != 0);
    // 发送LED状态
    outb(KEYBOARD_DATA_PORT, led_status);
}

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

// 处理键盘输入 - 增强版本
void handle_keyboard() {
    // 若键盘缓冲区无数据，直接返回
    if((inb(KEYBOARD_STATUS_PORT) & 1) == 0) {
        return;
    }
    
    // 读取键盘扫描码
    unsigned char scancode = inb(KEYBOARD_DATA_PORT);
    
    // 处理Shift键状态
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
        shift_pressed = 1;
        return;
    }
    
    if ((scancode == KEY_LSHIFT + 0x80) || (scancode == KEY_RSHIFT + 0x80)) {
        shift_pressed = 0;
        return;
    }
    
    // 处理CapsLock键
    if (scancode == KEY_CAPSLOCK) {
        caps_lock_on = !caps_lock_on;
        return;
    }
    
    // 如果是按键释放事件(高位为1)，忽略
    if (scancode & 0x80) {
        return;
    }
    
    // 最基本的按键映射
    char key = 0;
    
    // 处理常见按键 - 增加更多按键映射
    switch(scancode) {
        // 字母键 a-z (扫描码 0x1E-0x26, 0x2C-0x32)
        case 0x1E: key = shift_pressed || caps_lock_on ? 'A' : 'a'; break;
        case 0x30: key = shift_pressed || caps_lock_on ? 'B' : 'b'; break;
        case 0x2E: key = shift_pressed || caps_lock_on ? 'C' : 'c'; break;
        case 0x20: key = shift_pressed || caps_lock_on ? 'D' : 'd'; break;
        case 0x12: key = shift_pressed || caps_lock_on ? 'E' : 'e'; break;
        case 0x21: key = shift_pressed || caps_lock_on ? 'F' : 'f'; break;
        case 0x22: key = shift_pressed || caps_lock_on ? 'G' : 'g'; break;
        case 0x23: key = shift_pressed || caps_lock_on ? 'H' : 'h'; break;
        case 0x17: key = shift_pressed || caps_lock_on ? 'I' : 'i'; break;
        case 0x24: key = shift_pressed || caps_lock_on ? 'J' : 'j'; break;
        case 0x25: key = shift_pressed || caps_lock_on ? 'K' : 'k'; break;
        case 0x26: key = shift_pressed || caps_lock_on ? 'L' : 'l'; break;
        case 0x32: key = shift_pressed || caps_lock_on ? 'M' : 'm'; break;
        case 0x31: key = shift_pressed || caps_lock_on ? 'N' : 'n'; break;
        case 0x18: key = shift_pressed || caps_lock_on ? 'O' : 'o'; break;
        case 0x19: key = shift_pressed || caps_lock_on ? 'P' : 'p'; break;
        case 0x10: key = shift_pressed || caps_lock_on ? 'Q' : 'q'; break;
        case 0x13: key = shift_pressed || caps_lock_on ? 'R' : 'r'; break;
        case 0x1F: key = shift_pressed || caps_lock_on ? 'S' : 's'; break;
        case 0x14: key = shift_pressed || caps_lock_on ? 'T' : 't'; break;
        case 0x16: key = shift_pressed || caps_lock_on ? 'U' : 'u'; break;
        case 0x2F: key = shift_pressed || caps_lock_on ? 'V' : 'v'; break;
        case 0x11: key = shift_pressed || caps_lock_on ? 'W' : 'w'; break;
        case 0x2D: key = shift_pressed || caps_lock_on ? 'X' : 'x'; break;
        case 0x15: key = shift_pressed || caps_lock_on ? 'Y' : 'y'; break;
        case 0x2C: key = shift_pressed || caps_lock_on ? 'Z' : 'z'; break;
        
        // 数字键 0-9 (扫描码 0x02-0x0B)
        case 0x02: key = shift_pressed ? '!' : '1'; break;
        case 0x03: key = shift_pressed ? '@' : '2'; break;
        case 0x04: key = shift_pressed ? '#' : '3'; break;
        case 0x05: key = shift_pressed ? '$' : '4'; break;
        case 0x06: key = shift_pressed ? '%' : '5'; break;
        case 0x07: key = shift_pressed ? '^' : '6'; break;
        case 0x08: key = shift_pressed ? '&' : '7'; break;
        case 0x09: key = shift_pressed ? '*' : '8'; break;
        case 0x0A: key = shift_pressed ? '(' : '9'; break;
        case 0x0B: key = shift_pressed ? ')' : '0'; break;
        
        // 特殊按键
        case 0x1C: key = '\n'; break;  // Enter
        case 0x0E: key = '\b'; break;  // Backspace
        case 0x39: key = ' '; break;   // Space
        case 0x0F: key = '\t'; break;  // Tab
        
        // 符号键
        case 0x29: key = shift_pressed ? '~' : '`'; break;  // 反引号/波浪号
        case 0x0C: key = shift_pressed ? '_' : '-'; break;  // 减号/下划线
        case 0x0D: key = shift_pressed ? '+' : '='; break;  // 等号/加号
        case 0x1A: key = shift_pressed ? '{' : '['; break;  // 左方括号/左花括号
        case 0x1B: key = shift_pressed ? '}' : ']'; break;  // 右方括号/右花括号
        case 0x2B: key = shift_pressed ? '|' : '\\'; break; // 反斜杠/竖线
        case 0x27: key = shift_pressed ? ':' : ';'; break;  // 分号/冒号
        case 0x28: key = shift_pressed ? '"' : '\''; break; // 单引号/双引号
        case 0x33: key = shift_pressed ? '<' : ','; break;  // 逗号/小于号
        case 0x34: key = shift_pressed ? '>' : '.'; break;  // 句号/大于号
        case 0x35: key = shift_pressed ? '?' : '/'; break;  // 斜杠/问号
        
        default:
            // 忽略其他按键
            return;
    }
    
    // 处理输入
    if(key == '\n') {
        // 回车键，完成输入
        input_buffer[input_position] = '\0';
        print_char('\n');
        input_ready = 1;
    } else if(key == '\b') {
        // 退格键
        if(input_position > 0) {
            input_position--;
            print_char('\b');
            print_char(' ');  // 清除字符
            print_char('\b'); // 光标回退
        }
    } else if(key >= ' ' && key <= '~' && input_position < INPUT_BUFFER_SIZE - 1) {
        // 可打印字符，加入缓冲区
        input_buffer[input_position++] = key;
        print_char(key); // 显示字符
    }
}

// 读取用户输入
void read_input() {
    input_position = 0;
    input_ready = 0;
    
    // 超时计数器，不允许无限等待
    unsigned long timeout_counter = 0;
    const unsigned long MAX_TIMEOUT = 10000000; // 超时时间
    
    // 不再显示"Input ready"的提示，减少干扰
    
    while(!input_ready && timeout_counter < MAX_TIMEOUT) {
        // 检查键盘缓冲区
        if(inb(KEYBOARD_STATUS_PORT) & 1) {
            handle_keyboard();
        }
        
        // 小等待，减轻CPU负担
        for(int i = 0; i < 1000; i++) {
            __asm__("pause");
        }
        
        timeout_counter++;
        
        // 周期性处理键盘，确保即使上面逻辑失败也能检测输入
        if(timeout_counter % 100000 == 0) {
            handle_keyboard();
        }
    }
    
    // 处理超时情况
    if(timeout_counter >= MAX_TIMEOUT) {
        input_ready = 1;
        input_buffer[0] = '\0';
        
        // 显示超时信息
        set_text_color(RED_ON_BLACK);
        print_string("\nInput timeout! Press any key to continue.\n");
        set_text_color(WHITE_ON_BLACK);
        
        // 清除键盘缓冲区
        while(inb(KEYBOARD_STATUS_PORT) & 1) {
            inb(KEYBOARD_DATA_PORT);
        }
    }
}

// 初始化NTFS文件系统 - 增强版本，带错误处理
void init_ntfs() {
    if(!ntfs_initialized) {
        print_string("Initializing NTFS filesystem...\n");
        
        // 获取NTFS实例
        ntfs_fs_t* fs = get_ntfs_fs();
        
        // 设置初始化超时
        unsigned long start_time = get_tick();
        unsigned long timeout = 500; // 设置最长等待时间
        
        // 初始化NTFS文件系统
        print_string("Reading disk sectors...\n");
        int result = ntfs_init(fs, 2048);
        
        // 处理可能的错误
        if(result != 0) {
            set_text_color(RED_ON_BLACK);
            print_string("NTFS initialization failed, error code: ");
            print_int(result);
            print_newline();
            
            switch(result) {
                case -1:
                    print_string("Disk read error\n");
                    break;
                case -2:
                    print_string("Not a valid NTFS partition\n");
                    break;
                default:
                    print_string("Unknown error\n");
            }
            
            set_text_color(WHITE_ON_BLACK);
            print_string("System will continue with limited functionality\n");
            return;
        }
        
        ntfs_initialized = 1;
        print_string("NTFS initialization successful\n");
        
        // 尝试挂载文件系统
        print_string("Mounting NTFS filesystem...\n");
        result = ntfs_mount(fs);
        
        if(result != 0) {
            set_text_color(RED_ON_BLACK);
            print_string("NTFS mount failed, error code: ");
            print_int(result);
            print_newline();
            set_text_color(WHITE_ON_BLACK);
            print_string("File system operations may not work correctly\n");
        } else {
            print_string("NTFS filesystem mounted successfully\n");
        }
    }
}

// 确保NTFS已挂载
int ensure_ntfs_mounted() {
    ntfs_fs_t* fs = get_ntfs_fs();
    
    if(!ntfs_initialized) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: NTFS filesystem not initialized\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Use 'format' to initialize the NTFS filesystem\n");
        return 0;
    }
    
    if(!fs->mounted) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: NTFS filesystem not mounted\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Use 'mount' to mount the NTFS filesystem\n");
        return 0;
    }
    
    return 1;
}

// 命令: 帮助
void cmd_help() {
    set_text_color(BLUE_ON_BLACK);
    print_string("Available Commands:\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("  help      - Display this help information\n");
    print_string("  clear     - Clear the screen\n");
    print_string("  info      - Display system information\n");
    print_string("  about     - About ZZQ OS\n");
    print_string("  memory    - Display memory statistics\n");
    print_string("  ticks     - Display system clock ticks\n");
    
    set_text_color(BLUE_ON_BLACK);
    print_string("File System Commands (NTFS):\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("  format    - Format the NTFS filesystem\n");
    print_string("  mount     - Mount the NTFS filesystem\n");
    print_string("  unmount   - Unmount the NTFS filesystem\n");
    print_string("  ls        - List files\n");
    print_string("  touch     - Create an empty file\n");
    print_string("  cat       - Display file contents\n");
    print_string("  write     - Write content to a file\n");
    print_string("  rm        - Delete a file\n");
    
    set_text_color(BLUE_ON_BLACK);
    print_string("Keyboard Features:\n");
    set_text_color(WHITE_ON_BLACK);
    print_string("  Shift     - Use for uppercase letters and special characters\n");
    print_string("  Caps Lock - Toggle uppercase for letters\n");
    
    set_text_color(GREEN_ON_BLACK);
    print_string("\nNote: NTFS file system now has proper timeout handling for stability\n");
    set_text_color(WHITE_ON_BLACK);
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
    print_string("and NTFS filesystem support for persistent storage.\n\n");
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
    print_string("Filesystem: Basic (NTFS disabled)\n");
    print_string("Keyboard: Enhanced (Shift + CapsLock support)\n");
    print_string("System Uptime: ");
    print_int(get_tick());
    print_string(" ticks\n");
}

// 命令: 显示时钟计数
void cmd_ticks() {
    print_string("System uptime: ");
    print_int(get_tick());
    print_string(" ticks\n");
}

// 命令: 格式化NTFS分区
void cmd_format() {
    set_text_color(RED_ON_BLACK);
    print_string("Warning: This will format the NTFS partition and erase all data! Continue? (y/n): ");
    set_text_color(WHITE_ON_BLACK);
    
    read_input();
    
    if(input_buffer[0] == 'y' || input_buffer[0] == 'Y') {
        print_string("Formatting NTFS partition, please wait...\n");
        
        // 格式化NTFS分区
        int result = ntfs_format(2048, 100 * 1024 * 1024);
        
        if(result == 0) {
            ntfs_initialized = 1;
            print_string("NTFS partition formatted successfully!\n");
            
            // 自动初始化并挂载
            init_ntfs();
        } else {
            set_text_color(RED_ON_BLACK);
            print_string("NTFS format failed, error code: ");
            print_int(result);
            print_newline();
            set_text_color(WHITE_ON_BLACK);
        }
    } else {
        print_string("Format operation canceled.\n");
    }
}

// 命令: 挂载NTFS文件系统
void cmd_mount() {
    print_string("Mounting NTFS filesystem...\n");
    
    if(!ntfs_initialized) {
        init_ntfs();
        if(!ntfs_initialized) {
            set_text_color(RED_ON_BLACK);
            print_string("Error: NTFS initialization failed\n");
            set_text_color(WHITE_ON_BLACK);
            print_string("Use 'format' to initialize the NTFS filesystem\n");
            return;
        }
    }
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 挂载文件系统
    int result = ntfs_mount(fs);
    
    if(result == 0) {
        print_string("NTFS filesystem mounted successfully!\n");
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("NTFS mount failed, error code: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

// 命令: 卸载NTFS文件系统
void cmd_unmount() {
    print_string("Unmounting NTFS filesystem...\n");
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 卸载文件系统
    int result = ntfs_unmount(fs);
    
    if(result == 0) {
        print_string("NTFS filesystem unmounted.\n");
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("NTFS unmount failed, error code: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

// 命令: 列出文件
void cmd_ls() {
    // 检查NTFS是否已挂载
    if(!ensure_ntfs_mounted()) {
        return;
    }
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    print_string("Directory listing: /\n");
    
    // 列出目录内容
    ntfs_file_t* files = NULL;
    uint32_t count = 0;
    
    int result = ntfs_list_directory(fs, "/", &files, &count);
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Directory listing failed, error code: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    if(count == 0) {
        print_string("Directory is empty.\n");
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

// 命令: 创建空文件
void cmd_touch(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: touch <filename>\n");
        return;
    }
    
    // 检查NTFS是否已挂载
    if(!ensure_ntfs_mounted()) {
        return;
    }
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 创建空文件
    ntfs_file_t file;
    int result = ntfs_create_file(fs, filename, &file);
    
    if(result == 0) {
        print_string("File created: ");
        print_string(filename);
        print_newline();
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("File creation failed, error code: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

// 命令: 查看文件内容
void cmd_cat(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: cat <filename>\n");
        return;
    }
    
    // 检查NTFS是否已挂载
    if(!ensure_ntfs_mounted()) {
        return;
    }
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 查找文件
    ntfs_file_t file;
    int result = ntfs_find_file(fs, filename, &file);
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: File not found\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 分配缓冲区
    char* buffer = (char*)malloc(file.size + 1);
    if(!buffer) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Memory allocation failed\n");
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    // 读取文件内容
    result = ntfs_read_file(fs, &file, 0, file.size, buffer);
    
    if(result < 0) {
        set_text_color(RED_ON_BLACK);
        print_string("File read failed, error code: ");
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

// 命令: 写入文件
void cmd_write(const char* args) {
    char filename[256] = {0};
    
    // 解析文件名
    int i = 0;
    while(args[i] != ' ' && args[i] != '\0' && i < 255) {
        filename[i] = args[i];
        i++;
    }
    filename[i] = '\0';
    
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: write <filename> <content>\n");
        return;
    }
    
    // 跳过空格
    while(args[i] == ' ') {
        i++;
    }
    
    // 获取内容
    const char* content = &args[i];
    
    // 检查NTFS是否已挂载
    if(!ensure_ntfs_mounted()) {
        return;
    }
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 查找或创建文件
    ntfs_file_t file;
    if(ntfs_find_file(fs, filename, &file) != 0) {
        // 文件不存在，创建新文件
        int result = ntfs_create_file(fs, filename, &file);
        if(result != 0) {
            set_text_color(RED_ON_BLACK);
            print_string("File creation failed, error code: ");
            print_int(result);
            print_newline();
            set_text_color(WHITE_ON_BLACK);
            return;
        }
    }
    
    // 写入内容
    int content_length = strlen(content);
    int result = ntfs_write_file(fs, &file, 0, content_length, content);
    
    if(result != 0) {
        set_text_color(RED_ON_BLACK);
        print_string("File write failed, error code: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        return;
    }
    
    print_string("File written successfully: ");
    print_string(filename);
    print_string(" (");
    print_int(content_length);
    print_string(" bytes)\n");
}

// 命令: 删除文件
void cmd_rm(const char* filename) {
    if(strlen(filename) == 0) {
        set_text_color(RED_ON_BLACK);
        print_string("Error: Missing filename\n");
        set_text_color(WHITE_ON_BLACK);
        print_string("Usage: rm <filename>\n");
        return;
    }
    
    // 检查NTFS是否已挂载
    if(!ensure_ntfs_mounted()) {
        return;
    }
    
    // 获取NTFS实例
    ntfs_fs_t* fs = get_ntfs_fs();
    
    // 删除文件
    int result = ntfs_delete_file(fs, filename);
    
    if(result == 0) {
        print_string("File deleted: ");
        print_string(filename);
        print_newline();
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("File deletion failed, error code: ");
        print_int(result);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
    }
}

// 主函数
void kmain(void) {
    // 初始化内存管理系统
    memory_init();
    
    // 初始化定时器（100Hz）
    init_timer(100);
    
    // 清屏
    clear_screen();
    set_text_color(GREEN_ON_BLACK);
    print_string("ZZQ OS Starting...\n");
    print_string("Initializing keyboard controller...\n");
    set_text_color(WHITE_ON_BLACK);
    
    // 简化键盘初始化 - 避免过多的等待可能导致系统卡住
    
    // 初始化键盘控制器
    outb(KEYBOARD_COMMAND_PORT, 0xFF); // 发送复位命令
    
    // 短暂等待
    for(int i = 0; i < 1000; i++) {
        for(int j = 0; j < 1000; j++) {
            __asm__("pause");
        }
    }
    
    // 清空键盘缓冲区
    while(inb(KEYBOARD_STATUS_PORT) & 1) {
        inb(KEYBOARD_DATA_PORT);
    }
    
    // 设置键盘默认状态
    caps_lock_on = 0;
    shift_pressed = 0;
    
    // 启用键盘
    outb(KEYBOARD_COMMAND_PORT, 0xAE);
    print_string("Keyboard enabled\n");
    
    // 初始化NTFS文件系统
    print_string("Initializing NTFS filesystem...\n");
    init_ntfs();
    
    // 显示欢迎信息
    clear_screen();
    set_text_color(WHITE_ON_BLACK);
    print_string("Welcome to ZZQ OS VERSION ");
    print_string(VERSION);
    print_string("!\n");
    print_string("\nSimple OS with NTFS support for persistent storage\n");
    print_string("Copyright (c) 2023-2024\n\n");
    print_string("System is ready, please enter commands\n\n");
    
    // 主循环
    while(1) {
        print_prompt();
        read_input();
        parse_command(input_buffer);
    }
}

// 解析输入命令
void parse_command(char* command) {
    // 安全检查
    if(command == NULL) {
        return;
    }
    
    // 去除命令前后的空格
    while(*command == ' ') {
        command++;
    }
    
    // 检查是否为空命令
    if(*command == '\0') {
        return;
    }
    
    // 提取命令名
    char cmd_name[32] = {0};
    char args[INPUT_BUFFER_SIZE] = {0};
    
    int i = 0;
    while(command[i] != ' ' && command[i] != '\0' && i < 31) {
        cmd_name[i] = command[i];
        i++;
    }
    cmd_name[i] = '\0';
    
    // 提取参数(如果有)
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
    
    // 执行命令 - 使用字符串比较
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
    }
    // NTFS相关命令 - 重新启用
    else if(strcmp(cmd_name, "format") == 0) {
        cmd_format();
    } else if(strcmp(cmd_name, "mount") == 0) {
        cmd_mount();
    } else if(strcmp(cmd_name, "unmount") == 0) {
        cmd_unmount();
    } else if(strcmp(cmd_name, "ls") == 0) {
        cmd_ls();
    } else if(strcmp(cmd_name, "touch") == 0) {
        cmd_touch(args);
    } else if(strcmp(cmd_name, "cat") == 0) {
        cmd_cat(args);
    } else if(strcmp(cmd_name, "rm") == 0) {
        cmd_rm(args);
    } else if(strcmp(cmd_name, "write") == 0) {
        cmd_write(args);
    }
    // 兼容旧命令，重定向到新命令
    else if(strcmp(cmd_name, "ntfs_format") == 0) {
        cmd_format();
    } else if(strcmp(cmd_name, "ntfs_mount") == 0) {
        cmd_mount();
    } else if(strcmp(cmd_name, "ntfs_unmount") == 0) {
        cmd_unmount();
    } else if(strcmp(cmd_name, "ntfs_ls") == 0) {
        cmd_ls();
    } else if(strcmp(cmd_name, "ntfs_cat") == 0) {
        cmd_cat(args);
    } else if(strcmp(cmd_name, "ntfs_write") == 0) {
        cmd_write(args);
    } else if(strcmp(cmd_name, "ntfs_rm") == 0) {
        cmd_rm(args);
    } else {
        set_text_color(RED_ON_BLACK);
        print_string("Unknown command: ");
        print_string(cmd_name);
        print_newline();
        set_text_color(WHITE_ON_BLACK);
        print_string("Type 'help' for a list of available commands.\n");
    }
}

// 打印提示符
void print_prompt() {
    set_text_color(GREEN_ON_BLACK);
    print_string("ZZQ> ");
    set_text_color(WHITE_ON_BLACK);
} 