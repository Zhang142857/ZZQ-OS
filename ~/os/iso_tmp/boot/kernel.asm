; Multiboot 标准头部
MBALIGN     equ  1 << 0                ; 页对齐
MEMINFO     equ  1 << 1                ; 提供内存映射
FLAGS       equ  MBALIGN | MEMINFO     ; Multiboot 标志位
MAGIC       equ  0x1BADB002            ; Multiboot Magic Number
CHECKSUM    equ -(MAGIC + FLAGS)       ; 校验和

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

section .text
global _start
extern kmain

_start:
    mov esp, stack_top    ; 设置栈顶

    ; 调用内核
    call kmain

    ; 进入无限循环
    cli
.hang:
    hlt
    jmp .hang 