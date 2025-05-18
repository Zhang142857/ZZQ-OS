#include <stdint.h>

__attribute__((naked)) uint64_t __udivdi3(uint64_t a, uint64_t b) {
    asm("push %ebp");
    asm("mov %esp, %ebp");
    // 简化的32位除法实现
    asm("mov 8(%ebp), %eax");   // 加载被除数低32位
    asm("mov 12(%ebp), %edx");  // 加载被除数高32位
    asm("div 16(%ebp)");        // 除以除数低32位
    asm("mov %eax, 8(%ebp)");   // 将结果存回原位置
    asm("mov %ebp, %esp");
    asm("pop %ebp");
    asm("ret");
}

// 堆栈保护失败处理函数
void __stack_chk_fail(void) {
    // 简单实现，实际应终止程序
    while(1) {}
}
