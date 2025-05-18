[bits 16]
; 从磁盘加载DH个扇区到ES:BX
; 参数:
;   - DL: 驱动器号
;   - DH: 要读取的扇区数
;   - ES:BX: 加载目标的内存地址
disk_load:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    
    push dx          ; 存储DX，稍后比较读取的扇区数
    
    ; 设置重试计数器
    mov di, 5        ; 最多尝试5次
    
.retry:
    ; 重置磁盘系统
    xor ax, ax
    int 0x13        ; 重置磁盘系统
    jc .reset_failed ; 如果重置失败，跳转
    
    pop dx          ; 恢复DX
    push dx         ; 再次保存它用于比较
    
    ; 计算实际要读取的扇区数
    mov al, dh
    cmp al, 127
    jbe .read_sectors
    mov al, 127     ; 每次最多读取127个扇区
    
.read_sectors:
    mov ah, 0x02    ; BIOS读扇区功能
    ; AL已经设置为要读取的扇区数
    mov ch, 0x00    ; 柱面0
    mov dh, 0x00    ; 磁头0
    mov cl, 0x02    ; 从第二个扇区开始（引导扇区之后）
    int 0x13        ; BIOS中断
    
    jnc .read_success ; 如果没有进位标志，读取成功
    
    ; 读取失败，减少重试计数
    dec di
    jz disk_error   ; 如果重试次数用完，报错
    
    ; 否则重试
    pop dx          ; 恢复栈
    push dx
    jmp .retry

.reset_failed:
    pop dx          ; 恢复栈平衡
    jmp disk_error

.read_success:
    pop dx          ; 恢复DX
    cmp al, dh      ; 如果AL(读取的扇区)!=DH(期望的扇区)，显示错误
    jne sectors_error
    
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

disk_error:
    mov si, DISK_ERROR_MSG
    call print_string
    jmp disk_loop

sectors_error:
    mov si, SECTORS_ERROR_MSG
    call print_string
    
disk_loop:
    jmp $

DISK_ERROR_MSG db "Disk read error!", 0
SECTORS_ERROR_MSG db "Incorrect number of sectors read!", 0 