#include "custom_string.h"

typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;

__attribute__((naked)) uint64_t __udivdi3(uint64_t a, uint64_t b) {
    asm("push %ebp");
    asm("mov %esp, %ebp");
    // 这是一个极其简化的实现，用于解决链接错误
    asm("mov 8(%ebp), %eax");
    asm("mov 12(%ebp), %edx");
    asm("div 16(%ebp)");
    asm("mov %ebp, %esp");
    asm("pop %ebp");
    asm("ret");
}
