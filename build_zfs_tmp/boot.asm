; 多引导头部常量
MBALIGN     equ  1<<0                   ; 页对齐
MEMINFO     equ  1<<1                   ; 提供内存映射
FLAGS       equ  MBALIGN | MEMINFO      ; 这是多引导的"标志"字段
MAGIC       equ  0x1BADB002             ; "魔数"让引导程序找到头部
CHECKSUM    equ -(MAGIC + FLAGS)        ; 校验和

; 多引导标准不需要这个，但它对于bootloader不会覆盖我们的代码很有帮助
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; 内核入口点
section .text
[BITS 32]
[global _start]
[extern kmain]  ; kmain在C代码中定义

_start:
    ; 禁用中断
    cli
    
    ; 设置堆栈
    mov esp, stack_top
    
    ; 保存多引导信息
    push ebx
    
    ; 调用C内核
    call kmain
    
    ; 如果内核返回，就挂起
    cli
    hlt
_halt:
    jmp _halt

; 为堆栈预留空间
section .bss
align 16
stack_bottom:
    resb 32768 ; 32KB堆栈
stack_top:
