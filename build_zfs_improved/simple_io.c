// 简单的I/O函数

// 写一个字节到屏幕
void print_char(char c) {
    // 假设屏幕输出内存地址为0xB8000 (标准VGA文本模式)
    static int x = 0;
    static int y = 0;
    
    unsigned char* video_memory = (unsigned char*)0xB8000;
    
    if (c == '\n') {
        x = 0;
        y++;
        return;
    }
    
    // 计算当前位置
    int position = y * 80 + x;
    
    // 写入字符和属性（白色文字，黑色背景）
    video_memory[position * 2] = c;
    video_memory[position * 2 + 1] = 0x07;
    
    // 更新位置
    x++;
    if (x >= 80) {
        x = 0;
        y++;
    }
    
    // 简单滚屏
    if (y >= 25) {
        for (int i = 0; i < 24 * 80 * 2; i++) {
            video_memory[i] = video_memory[i + 80 * 2];
        }
        
        // 清除最后一行
        for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i + 1] = 0x07;
        }
        
        y = 24;
    }
}

// 打印字符串
void print_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i]);
    }
}

// 打印整数
void print_int(int num) {
    char buffer[12];
    int i = 0;
    int negative = 0;
    
    if (num == 0) {
        print_char('0');
        return;
    }
    
    if (num < 0) {
        negative = 1;
        num = -num;
    }
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    if (negative) {
        print_char('-');
    }
    
    for (int j = i - 1; j >= 0; j--) {
        print_char(buffer[j]);
    }
}

// 打印换行
void print_newline(void) {
    print_char('\n');
}
