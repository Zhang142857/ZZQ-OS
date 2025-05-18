#include "timer.h"

// 外部函数声明
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// 定义PIT相关端口
#define PIT_CHANNEL0_DATA    0x40
#define PIT_COMMAND          0x43
#define PIT_FREQUENCY        1193180  // PIT频率（每秒）

// 定义CMOS/RTC端口
#define CMOS_ADDRESS         0x70
#define CMOS_DATA            0x71

// 系统开机以来的滴答计数
static volatile unsigned int tick_count = 0;

// 中断处理程序的声明（在boot.asm中）
extern void isr_timer();

// 初始化PIT
void init_timer(unsigned int frequency) {
    // 计算分频值
    unsigned int divisor = PIT_FREQUENCY / frequency;
    
    // 发送初始化命令
    outb(PIT_COMMAND, 0x36);  // 通道0，方式3，二进制
    
    // 设置分频值（低8位和高8位）
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
    outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);
}

// 更新滴答计数（由中断处理程序调用）
void timer_tick() {
    tick_count++;
}

// 获取滴答计数
unsigned int get_tick_count() {
    return tick_count;
}

// 等待指定毫秒数
void sleep(unsigned int ms) {
    unsigned int end_tick = tick_count + ms;
    while(tick_count < end_tick) {
        // 等待
        __asm__ volatile("hlt");
    }
}

// 将BCD码转换为二进制
unsigned char bcd_to_binary(unsigned char bcd) {
    return ((bcd & 0xF0) >> 4) * 10 + (bcd & 0x0F);
}

// 读取CMOS寄存器
unsigned char read_cmos(unsigned char reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

// 读取RTC时间
void read_rtc(unsigned char* hour, unsigned char* minute, unsigned char* second) {
    unsigned char status_b;
    
    // 读取CMOS状态寄存器B，确定时间格式（12小时制或24小时制）
    status_b = read_cmos(0x0B);
    
    // 读取秒、分、时
    *second = readcmos(0x00);
    *minute = read_cmos(0x02);
    *hour = read_cmos(0x04);
    
    // 如果是BCD格式，转换为二进制
    if (!(status_b & 0x04)) {
        *second = bcd_to_binary(*second);
        *minute = bcd_to_binary(*minute);
        *hour = bcd_to_binary(*hour);
    }
    
    // 如果是12小时制，转换为24小时制
    if (!(status_b & 0x02) && (*hour & 0x80)) {
        *hour = ((*hour & 0x7F) + 12) % 24;
    }
} 