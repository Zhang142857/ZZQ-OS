#!/bin/bash
# 简化的ISO构建脚本

# 设置版本号（每次需要构建新ISO时可以手动递增此值）
VERSION="V1.0"
ISO_NAME="zzqos_${VERSION}.iso"
ZIP_NAME="zzqos_${VERSION}.zip"

# 用于输出消息的函数
echo_info() {
    echo -e "\033[1;36m[INFO]\033[0m $1"
}

echo_success() {
    echo -e "\033[1;32m[SUCCESS]\033[0m $1"
}

echo_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

echo_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

# 显示标题
echo -e "\033[1;34m"
echo "========================================"
echo "  ZZQ-OS 构建脚本"
echo "  当前版本: $VERSION"
echo "========================================"
echo -e "\033[0m"

echo_info "开始构建ZZQ-OS ${VERSION}..."

# 确保清理之前的构建文件
if [ -d "kernel" ]; then
    echo_info "清理之前的构建文件..."
    rm -rf kernel/*.o kernel/*.elf kernel/kernel.bin 2>/dev/null
else
    mkdir -p kernel
fi

# 编译内核入口
nasm -f elf kernel/boot.asm -o kernel/kernel_entry.o

# 创建简化的memory.c来避免外部依赖
echo_info "创建内存管理模块..."
cat > kernel/simple_memory.c << 'EOF'
#include "memory.h"
#include <stdint.h>

// 内存映射类型
#define MEMORY_FREE 1
#define MEMORY_RESERVED 2
#define MEMORY_ACPI_RECLAIMABLE 3
#define MEMORY_NVS 4
#define MEMORY_BADRAM 5

// 内存页大小 (4KB)
#define PAGE_SIZE 4096

// 页的状态
#define PAGE_FREE 0
#define PAGE_USED 1

// 内存管理结构
static struct {
    uint32_t* page_bitmap;
    uint32_t pages_count;
    uint32_t bitmap_size;
    uint32_t used_pages;
    uint32_t free_pages;
} memory_manager;

// 外部依赖声明
extern void print_string(const char* str);
extern void print_newline(void);
extern void int_to_string(int num, char* str);
extern void print_int(int num);

// 设置位图中的位
void bitmap_set(uint32_t* bitmap, uint32_t bit) {
    bitmap[bit / 32] |= (1 << (bit % 32));
}

// 清除位图中的位
void bitmap_clear(uint32_t* bitmap, uint32_t bit) {
    bitmap[bit / 32] &= ~(1 << (bit % 32));
}

// 测试位图中的位
int bitmap_test(uint32_t* bitmap, uint32_t bit) {
    return bitmap[bit / 32] & (1 << (bit % 32));
}

// 初始化内存管理系统
void memory_init() {
    // 简化版：仅初始化基本结构
    uint32_t memory_size = 16 * 1024 * 1024; // 假设有16MB内存
    memory_manager.pages_count = memory_size / PAGE_SIZE;
    memory_manager.bitmap_size = (memory_manager.pages_count + 31) / 32;
    
    // 将位图放在内核之后的内存中(简化版，假设地址)
    memory_manager.page_bitmap = (uint32_t*)0x200000; // 改为2MB，避免与内核冲突
    
    // 将所有页面标记为可用
    for (uint32_t i = 0; i < memory_manager.bitmap_size; i++) {
        memory_manager.page_bitmap[i] = 0;
    }
    
    memory_manager.free_pages = memory_manager.pages_count;
    memory_manager.used_pages = 0;
    
    // 将内核和位图所占页面标记为已使用
    uint32_t bitmap_pages = (memory_manager.bitmap_size * 4 + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < 256 + bitmap_pages; i++) {
        bitmap_set(memory_manager.page_bitmap, i);
        memory_manager.used_pages++;
        memory_manager.free_pages--;
    }
}

// 分配物理页面
void* memory_alloc_page() {
    if (memory_manager.free_pages == 0) {
        return 0; // 内存不足
    }
    
    for (uint32_t i = 0; i < memory_manager.pages_count; i++) {
        if (!bitmap_test(memory_manager.page_bitmap, i)) {
            bitmap_set(memory_manager.page_bitmap, i);
            memory_manager.used_pages++;
            memory_manager.free_pages--;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    return 0; // 未找到空闲页面
}

// 释放物理页面
void memory_free_page(void* addr) {
    uint32_t page = (uint32_t)addr / PAGE_SIZE;
    if (page < memory_manager.pages_count && bitmap_test(memory_manager.page_bitmap, page)) {
        bitmap_clear(memory_manager.page_bitmap, page);
        memory_manager.used_pages--;
        memory_manager.free_pages++;
    }
}

// 内存统计信息
void memory_stats() {
    print_string("Memory statistics:\n");
    
    print_string("  Total memory: ");
    print_int(memory_manager.pages_count * PAGE_SIZE / 1024);
    print_string(" KB (");
    print_int(memory_manager.pages_count);
    print_string(" pages)\n");
    
    print_string("  Used memory:  ");
    print_int(memory_manager.used_pages * PAGE_SIZE / 1024);
    print_string(" KB (");
    print_int(memory_manager.used_pages);
    print_string(" pages)\n");
    
    print_string("  Free memory:  ");
    print_int(memory_manager.free_pages * PAGE_SIZE / 1024);
    print_string(" KB (");
    print_int(memory_manager.free_pages);
    print_string(" pages)\n");
}

// 分配内存块
void* malloc(unsigned int size) {
    if (size == 0) {
        return 0;
    }
    
    // 计算需要的页数
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // 分配一个页面
    return memory_alloc_page();
}

// 释放内存块
void free(void* ptr) {
    if (ptr) {
        memory_free_page(ptr);
    }
}
EOF

# 创建简化的timer.c
echo_info "创建定时器模块..."
cat > kernel/simple_timer.c << 'EOF'
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
EOF

# 创建简化的string.c
echo_info "创建字符串处理模块..."
cat > kernel/string.c << 'EOF'
#include "string.h"

// 字符串复制函数
void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++) != 0);
}

// 字符串比较函数
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 比较指定长度的字符串
int strncmp(const char* s1, const char* s2, unsigned int n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return n == (unsigned int)-1 ? 0 : *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 字符串长度函数
unsigned int strlen(const char* str) {
    const char* s = str;
    while (*s) s++;
    return s - str;
}

// 内存复制函数
void memcpy(void* dest, const void* src, unsigned int n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
}

// 内存填充函数
void memset(void* ptr, int value, unsigned int n) {
    unsigned char* p = (unsigned char*)ptr;
    while (n--) *p++ = (unsigned char)value;
}

// 比较内存区域
int memcmp(const void* s1, const void* s2, unsigned int n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}
EOF

# 创建简化的string.h头文件
echo_info "创建头文件..."
cat > kernel/string.h << 'EOF'
#ifndef STRING_H
#define STRING_H

// 字符串复制函数
void strcpy(char* dest, const char* src);

// 字符串比较函数
int strcmp(const char* s1, const char* s2);

// 比较指定长度的字符串
int strncmp(const char* s1, const char* s2, unsigned int n);

// 字符串长度函数
unsigned int strlen(const char* str);

// 内存复制函数
void memcpy(void* dest, const void* src, unsigned int n);

// 内存填充函数
void memset(void* ptr, int value, unsigned int n);

// 在字符串中查找字符的第一次出现
char* strchr(const char* s, int c);

// 在字符串中查找字符的最后一次出现
char* strrchr(const char* s, int c);

// 将指定长度的字符串复制到目标字符串
char* strncpy(char* dest, const char* src, unsigned int n);

// 比较内存区域
int memcmp(const void* s1, const void* s2, unsigned int n);

#endif // STRING_H
EOF

# 创建简化的memory.h头文件
cat > kernel/memory.h << 'EOF'
#ifndef MEMORY_H
#define MEMORY_H

// 内存管理功能
void memory_init();
void* memory_alloc_page();
void memory_free_page(void* addr);
void memory_stats();

// 内存分配
void* malloc(unsigned int size);
void free(void* ptr);

#endif // MEMORY_H
EOF

# 创建简化的timer.h头文件
cat > kernel/timer.h << 'EOF'
#ifndef TIMER_H
#define TIMER_H

// 初始化定时器
void init_timer(unsigned int freq);

// 获取当前时钟计数
unsigned int get_tick();

#endif // TIMER_H
EOF

# 创建内核入口文件
echo_info "创建内核入口点..."
cat > kernel/boot.asm << 'EOF'
; 多引导头部常量
MBALIGN     equ  1<<0                   ; 页对齐
MEMINFO     equ  1<<1                   ; 提供内存映射
FLAGS       equ  MBALIGN | MEMINFO      ; 这是多引导的"标志"字段
MAGIC       equ  0x1BADB002             ; "魔数"让引导程序找到头部
CHECKSUM    equ -(MAGIC + FLAGS)        ; 校验和

; 多引导标准不需要这个，但它对于bootloader不会覆盖我们的代码很有帮助
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; 内核入口点
section .text
[BITS 32]
[global _start]
[extern kmain]  ; kmain在C代码中定义

_start:
    ; 禁用中断
    cli
    
    ; 设置堆栈
    mov esp, stack_top
    
    ; 保存多引导信息
    push ebx
    
    ; 调用C内核
    call kmain
    
    ; 如果内核返回，就挂起
    cli
    hlt
_halt:
    jmp _halt

; 为堆栈预留空间
section .bss
align 16
stack_bottom:
    resb 32768 ; 32KB堆栈
stack_top:
EOF

# 创建链接器脚本
echo_info "创建链接器脚本..."
cat > kernel/linker.ld << 'EOF'
/* 内核将从1MB(0x100000)处加载，这是GRUB期望多引导内核的地址 */
ENTRY(_start)
SECTIONS
{
    /* 多引导标准指定入口点必须高于1MB */
    . = 1M;

    /* 首先放置多引导头部，确保它在前8KB内 */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* 只读数据段 */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    /* 读写数据段 */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    /* BSS段（未初始化数据） */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
    
    /* 保留一些空间放置页表等数据结构 */
    . = ALIGN(4K);
    _end = .;
    
    /* 请求内核至少32MB空间以避免某些VMware版本的问题 */
    . = . + 32M;
}
EOF

# 编译内核入口
echo_info "编译内核入口点..."
mkdir -p boot
nasm -f elf32 -o kernel/kernel_entry.o kernel/boot.asm || { echo_error "编译内核入口失败"; exit 1; }

# 编译简化的内核源文件
echo_info "编译内核源文件..."
gcc -ffreestanding -fno-pie -m32 -c kernel/simple_memory.c -o kernel/memory.o || { echo_error "编译内存管理模块失败"; exit 1; }
gcc -ffreestanding -fno-pie -m32 -c kernel/simple_timer.c -o kernel/timer.o || { echo_error "编译定时器模块失败"; exit 1; }
gcc -ffreestanding -fno-pie -m32 -c kernel/simple_kernel.c -o kernel/kernel.o || { echo_error "编译内核模块失败"; exit 1; }

# 编译string.c
gcc -ffreestanding -fno-pie -m32 -c kernel/string.c -o kernel/string.o || { echo_error "编译字符串模块失败"; exit 1; }

# 链接内核 - 保留ELF格式供GRUB使用
echo_info "链接内核..."
ld -m elf_i386 -o kernel/kernel.elf -T kernel/linker.ld kernel/kernel_entry.o kernel/kernel.o kernel/memory.o kernel/timer.o kernel/string.o || { echo_error "链接内核失败"; exit 1; }

# 创建ISO镜像
echo_info "创建ISO镜像 ${ISO_NAME}..."
mkdir -p iso_tmp/boot/grub
cp kernel/kernel.elf iso_tmp/boot/kernel.elf
echo "menuentry \"ZZQ-OS ${VERSION}\" {" > iso_tmp/boot/grub/grub.cfg
echo "  multiboot /boot/kernel.elf" >> iso_tmp/boot/grub/grub.cfg
echo "  boot" >> iso_tmp/boot/grub/grub.cfg
echo "}" >> iso_tmp/boot/grub/grub.cfg

grub-mkrescue -o "$ISO_NAME" iso_tmp || { echo_error "创建ISO镜像失败"; exit 1; }

# 清理临时文件
rm -rf iso_tmp

echo_success "成功创建ISO镜像: ${ISO_NAME}"
echo_info "您现在可以在VMware中运行此ISO了！"