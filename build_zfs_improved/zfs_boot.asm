; ZFS 引导扇区
; 多引导标准兼容头部
section .multiboot
align 4
    dd 0x1BADB002                ; Multiboot magic
    dd 0x00000003                ; Multiboot 标志 (页对齐 + 内存信息)
    dd 0xE4524FFD                ; Multiboot 校验和

global _start
section .text
_start:
    ; 禁用中断
    cli
    
    ; 设置堆栈
    mov esp, stack_top
    
    ; 跳转到内核
    jmp kernel_main

; Multiboot 引导的内核入口点
global kernel_main
extern kmain
kernel_main:
    ; 将多引导信息传递给 C 内核
    push ebx
    
    ; 调用 C 内核
    call kmain
    
    ; 如果内核返回，则挂起
    cli
    hlt
.hang:
    jmp .hang

; 为堆栈预留空间
section .bss
align 16
stack_bottom:
    resb 16384 ; 16KB 堆栈
stack_top: 