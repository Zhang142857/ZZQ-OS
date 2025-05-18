#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h> // 添加这个，为了NULL定义

// 内存块结构体
typedef struct memory_block {
    unsigned int size;
    unsigned char is_free;
    struct memory_block* next;
} memory_block_t;

// 内存管理功能
void memory_init();
void* memory_alloc_page();
void memory_free_page(void* addr);
void memory_stats();

// 内存分配
void* malloc(unsigned int size);
void free(void* ptr);

#endif // MEMORY_H
