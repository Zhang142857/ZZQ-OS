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
