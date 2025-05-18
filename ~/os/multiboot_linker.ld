ENTRY(start)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)

SECTIONS
{
    . = 1M;  /* 从1MB处开始，避免BIOS区域 */
    
    .text : 
    {
        *(.multiboot)   /* 多重引导头 */
        *(.text)        /* 所有代码段 */
    }
    
    .rodata ALIGN(4K) : 
    {
        *(.rodata)      /* 只读数据 */
    }
    
    .data ALIGN(4K) : 
    {
        *(.data)        /* 已初始化数据 */
    }
    
    .bss ALIGN(4K) : 
    {
        *(COMMON)       /* 通用段 */
        *(.bss)         /* 未初始化数据 */
    }
} 