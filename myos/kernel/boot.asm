; multiboot 规范
MBALIGN     equ  1<<0                   ; 页对齐
MEMINFO     equ  1<<1                   ; 提供内存映射
FLAGS       equ  MBALIGN | MEMINFO      ; multiboot 标志
MAGIC       equ  0x1BADB002             ; multiboot 魔数
CHECKSUM    equ -(MAGIC + FLAGS)        ; 校验和

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .text
global _start
extern kernel_main

_start:
    cli                      ; 禁用中断
    mov esp, stack_top       ; 设置栈指针
    call kernel_main         ; 调用C内核入口
    hlt                      ; 永远停止CPU

section .bss
align 16
stack_bottom:
    resb 16384          ; 16KB栈
stack_top:
