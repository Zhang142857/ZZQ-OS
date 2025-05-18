// 简单的内存管理实现

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

// 内存分配（简化版）
void* kmalloc(uint32_t size) {
    // 这是一个非常简化的实现，实际上会导致内存泄漏
    static uint8_t memory[1024 * 1024]; // 1MB的静态内存池
    static uint32_t next_free = 0;
    
    uint32_t allocated = next_free;
    next_free += size;
    
    // 安全检查
    if (next_free >= sizeof(memory)) {
        return 0; // 内存不足
    }
    
    return (void*)&memory[allocated];
}

// 内存释放（简化版，实际上不做任何事）
void kfree(void* ptr) {
    // 在这个简化实现中，我们不会回收内存
}
