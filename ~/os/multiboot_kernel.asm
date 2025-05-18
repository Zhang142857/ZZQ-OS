; Multiboot头部定义
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_PAGE_ALIGN    equ (1 << 0)
MBOOT_MEM_INFO      equ (1 << 1)
MBOOT_HEADER_FLAGS  equ (MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO)
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

bits 32
section .text

; Multiboot头
align 4
dd MBOOT_HEADER_MAGIC
dd MBOOT_HEADER_FLAGS
dd MBOOT_CHECKSUM

; 内核入口点
global start
extern kmain

start:
    ; 设置栈
    mov esp, stack_space
    
    ; 调用C内核
    call kmain
    
    ; 如果内核返回，进入无限循环
    cli
    hlt
    jmp $

section .bss
    align 4
    resb 8192 ; 8KB栈空间
stack_space: 