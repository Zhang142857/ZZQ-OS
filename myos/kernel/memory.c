#include "memory.h"

// 初始堆大小（64KB）
#define HEAP_SIZE (64 * 1024)

// 内存对齐到8字节
#define ALIGN(x) (((x) + 7) & ~7)

// 块头部大小
#define BLOCK_HEADER_SIZE ALIGN(sizeof(memory_block_t))

// 堆区域
static char heap[HEAP_SIZE];
static memory_block_t* free_list = NULL;

// 将int转换为字符串
void int_to_string(int num, char* str) {
    int i = 0;
    int is_negative = 0;
    
    // 处理负数
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // 处理0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    // 转换数字到字符串（反向）
    while (num != 0) {
        str[i++] = (num % 10) + '0';
        num = num / 10;
    }
    
    // 添加负号（如果需要）
    if (is_negative) {
        str[i++] = '-';
    }
    
    // 添加字符串结束符
    str[i] = '\0';
    
    // 反转字符串
    int start = 0;
    int end = i - 1;
    char temp;
    
    while (start < end) {
        temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// 初始化内存管理系统
void memory_init() {
    free_list = (memory_block_t*)heap;
    free_list->size = HEAP_SIZE - BLOCK_HEADER_SIZE;
    free_list->is_free = 1;
    free_list->next = NULL;
}

// 分配内存
void* malloc(unsigned int size) {
    memory_block_t* current = free_list;
    memory_block_t* previous = NULL;
    
    // 对齐内存大小
    size = ALIGN(size);
    
    // 查找足够大的空闲块
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            // 分配大小完全吻合或剩余内存不足以创建新块
            if (current->size <= size + BLOCK_HEADER_SIZE) {
                current->is_free = 0;
            } 
            // 分割当前块
            else {
                memory_block_t* new_block = (memory_block_t*)((char*)current + BLOCK_HEADER_SIZE + size);
                new_block->size = current->size - size - BLOCK_HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->is_free = 0;
                current->next = new_block;
            }
            
            // 返回分配的内存
            return (void*)((char*)current + BLOCK_HEADER_SIZE);
        }
        
        previous = current;
        current = current->next;
    }
    
    // 没有找到足够大的块
    return NULL;
}

// 释放内存
void free(void* ptr) {
    if (ptr == NULL) return;
    
    // 获取块头
    memory_block_t* block = (memory_block_t*)((char*)ptr - BLOCK_HEADER_SIZE);
    
    // 标记为空闲
    block->is_free = 1;
    
    // 合并相邻的空闲块（简单实现，不处理所有情况）
    memory_block_t* current = free_list;
    
    while (current != NULL) {
        if (current->is_free && current->next != NULL && current->next->is_free) {
            current->size += BLOCK_HEADER_SIZE + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// 打印内存状态（用于调试）
void memory_stats() {
    memory_block_t* current = free_list;
    int total_blocks = 0;
    int free_blocks = 0;
    unsigned int total_free_space = 0;
    
    // 计算内存统计信息
    while (current != NULL) {
        total_blocks++;
        if (current->is_free) {
            free_blocks++;
            total_free_space += current->size;
        }
        current = current->next;
    }
    
    // 转换数字为字符串
    char total_blocks_str[16];
    char free_blocks_str[16];
    char total_free_space_str[16];
    
    int_to_string(total_blocks, total_blocks_str);
    int_to_string(free_blocks, free_blocks_str);
    int_to_string(total_free_space, total_free_space_str);
    
    // 借用kernel.c中的打印函数，这里声明为外部函数
    extern void print_string(const char* str);
    extern void print_newline(void);
    
    print_string("Memory Statistics:");
    print_newline();
    print_string("  Total blocks: ");
    print_string(total_blocks_str);
    print_newline();
    print_string("  Free blocks: ");
    print_string(free_blocks_str);
    print_newline();
    print_string("  Total free space: ");
    print_string(total_free_space_str);
    print_string(" bytes");
    print_newline();
} 