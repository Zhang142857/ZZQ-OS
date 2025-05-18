[bits 16]
; 使用BIOS中断打印SI指向的以0结尾的字符串
print_string:
    push ax
    push si
    
    mov ah, 0x0e    ; BIOS中断，显示字符
.loop:
    lodsb           ; 从SI加载字节到AL并递增SI
    or al, al       ; 检查是否为0（字符串结束）
    jz .done
    int 0x10        ; 显示AL中的字符
    jmp .loop
.done:
    pop si
    pop ax
    ret 