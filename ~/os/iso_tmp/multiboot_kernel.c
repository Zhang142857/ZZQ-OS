#define VIDEO_MEMORY 0xb8000
#define ROWS 25
#define COLS 80
#define WHITE_ON_BLUE 0x1F

// 屏幕光标位置
int cursor_x = 0;
int cursor_y = 0;

// 简单的端口输出函数
void outb(unsigned short port, unsigned char data) {
    __asm__("out %%al, %%dx" : : "a" (data), "d" (port));
}

// 更新光标位置
void update_cursor() {
    unsigned short position = cursor_y * COLS + cursor_x;
    
    // 光标位置低位
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
    
    // 光标位置高位
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

// 清屏
void clear_screen() {
    char* video_memory = (char*)VIDEO_MEMORY;
    
    for(int i = 0; i < ROWS * COLS; i++) {
        video_memory[i*2] = ' ';
        video_memory[i*2+1] = WHITE_ON_BLUE;
    }
    
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

// 打印字符
void print_char(char c) {
    char* video_memory = (char*)VIDEO_MEMORY;
    
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
        // 滚动屏幕
        for(int i = 0; i < (ROWS-1) * COLS; i++) {
            video_memory[i*2] = video_memory[(i + COLS) * 2];
            video_memory[i*2+1] = video_memory[(i + COLS) * 2 + 1];
        }
        
        // 清除最后一行
        for(int i = (ROWS-1) * COLS; i < ROWS * COLS; i++) {
            video_memory[i*2] = ' ';
            video_memory[i*2+1] = WHITE_ON_BLUE;
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

// 主函数 - 入口点
void kmain(void) {
    clear_screen();
    print_string("Welcome to ZZQ OS (VMware Edition)!");
    print_char('\n');
    print_string("This ISO image is bootable in VMware and other virtual machines.");
    print_char('\n');
    print_char('\n');
    print_string("ZZQ OS v1.0 - ISO Edition");
    
    // 无限循环
    while(1) {}
} 