// 简单的定时器实现

typedef unsigned int uint32_t;

// 获取系统时间计数器（简化版）
uint32_t get_tick(void) {
    // 这里应该读取实际的时间戳寄存器，但我们简化为静态计数器
    static uint32_t tick = 0;
    return tick++;
}
