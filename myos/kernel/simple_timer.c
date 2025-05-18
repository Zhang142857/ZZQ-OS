#include "timer.h"

// 外部函数
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// 时钟中断频率
unsigned int tick = 0;

// 初始化定时器
void init_timer(unsigned int freq) {
    // 将系统时钟分配给定的频率
    unsigned int divisor = 1193180 / freq;
    
    // 发送命令
    outb(0x43, 0x36);
    
    // 发送分频因子
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

// 获取当前时钟计数
unsigned int get_tick() {
    return tick;
}
