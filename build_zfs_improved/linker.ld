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
