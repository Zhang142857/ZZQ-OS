#define VIDEO_MEMORY 0xB8000
#define COLS 80
#define ROWS 25
#define WHITE_ON_BLUE 0x1F

// 屏幕光标位置
int cursor_x = 0;
int cursor_y = 0;

// 端口读写函数
void outb(unsigned short port, unsigned char data) {
    __asm__ volatile ("outb %0, %1" : : "a" (data), "Nd" (port));
}

unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a" (ret) : "Nd" (port));
    return ret;
}

// 更新光标位置
void update_cursor() {
    unsigned short position = cursor_y * COLS + cursor_x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
    
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

// 打印单个字符
void print_char(char c) {
    unsigned char* video_memory = (unsigned char*)VIDEO_MEMORY;
    
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        video_memory[(cursor_y * COLS + cursor_x) * 2] = c;
        video_memory[(cursor_y * COLS + cursor_x) * 2 + 1] = WHITE_ON_BLUE;
        cursor_x++;
    }
    
    if (cursor_x >= COLS) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= ROWS) {
        // 简单处理屏幕滚动
        for (int i = 0; i < ROWS-1; i++) {
            for (int j = 0; j < COLS*2; j++) {
                video_memory[i * COLS * 2 + j] = video_memory[(i+1) * COLS * 2 + j];
            }
        }
        
        // 清除最后一行
        for (int j = 0; j < COLS; j++) {
            video_memory[(ROWS-1) * COLS * 2 + j*2] = ' ';
            video_memory[(ROWS-1) * COLS * 2 + j*2 + 1] = WHITE_ON_BLUE;
        }
        
        cursor_y = ROWS - 1;
    }
    
    update_cursor();
}

// 打印字符串
void print_string(const char* str) {
    int i = 0;
    while (str[i]) {
        print_char(str[i++]);
    }
}

// 清屏
void clear_screen() {
    unsigned char* video_memory = (unsigned char*)VIDEO_MEMORY;
    
    for (int i = 0; i < ROWS * COLS; i++) {
        video_memory[i*2] = ' ';
        video_memory[i*2 + 1] = WHITE_ON_BLUE;
    }
    
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

// 内核主函数
void kmain() {
    clear_screen();
    
    print_string("Welcome to ZZQ OS v1.0");
    print_char('\n');
    print_string("------------------------");
    print_char('\n');
    print_char('\n');
    print_string("Successfully booted with GRUB!");
    
    // 无限循环
    while (1) {}
} 