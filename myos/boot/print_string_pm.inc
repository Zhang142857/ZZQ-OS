[bits 32]
; 使用VGA内存直接打印EBX指向的以0结尾的字符串
; 参数:
;   - EBX: 字符串地址
VIDEO_MEMORY equ 0xb8000
WHITE_ON_BLACK equ 0x0f

print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY
    
.loop:
    mov al, [ebx]       ; 存储字符
    mov ah, WHITE_ON_BLACK ; 存储属性
    
    cmp al, 0           ; 检查是否到字符串结尾
    je .done
    
    mov [edx], ax       ; 存储字符+属性到VGA内存
    
    add ebx, 1          ; 字符串+1
    add edx, 2          ; VGA内存+2
    
    jmp .loop
    
.done:
    popa
    ret 