; 16位启动扇区
BITS 16
ORG 0x7c00

; 初始化段寄存器
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; 显示启动消息
mov si, boot_msg
call print_string

; 加载内核
mov ah, 0x02    ; 读取扇区功能
mov al, 32      ; 读取32个扇区(16KB)
mov ch, 0       ; 柱面0
mov cl, 2       ; 扇区2（从1开始计数）
mov dh, 0       ; 磁头0
mov dl, 0x80    ; 驱动器号（第一个硬盘）
mov bx, 0x1000  ; 缓冲区地址
mov es, bx
mov bx, 0

int 0x13        ; BIOS中断
jc error        ; 如果出错，跳转到错误处理

; 准备进入保护模式
cli             ; 禁用中断
lgdt [gdt_desc] ; 加载GDT

; 启用A20线
in al, 0x92
or al, 2
out 0x92, al

; 切换到保护模式
mov eax, cr0
or eax, 1
mov cr0, eax

; 执行长跳转到32位代码
jmp 0x08:protected_mode

; 打印字符串函数
print_string:
    push ax
    push bx
    mov ah, 0x0e    ; BIOS teletype输出
    mov bh, 0       ; 页号
.loop:
    lodsb           ; 从DS:SI加载字节到AL并增加SI
    or al, al       ; 测试是否到字符串结尾
    jz .done
    int 0x10        ; 打印字符
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; 错误处理
error:
    mov si, error_msg
    call print_string
    jmp $

; 进入保护模式后的代码
BITS 32
protected_mode:
    ; 初始化段寄存器
    mov ax, 0x10    ; 数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000 ; 设置栈指针

    ; 跳转到内核
    jmp 0x08:0x10000

; GDT表
gdt:
    ; 空描述符
    dw 0, 0, 0, 0
    
    ; 代码段描述符
    dw 0xffff    ; 段限长度（低16位）
    dw 0x0000    ; 段基址（低16位）
    db 0x00      ; 段基址（中8位）
    db 10011010b ; 存在、特权级0、可执行、可读
    db 11001111b ; 粒度4KB、32位保护模式、段限长度（高4位）
    db 0x00      ; 段基址（高8位）
    
    ; 数据段描述符
    dw 0xffff    ; 段限长度（低16位）
    dw 0x0000    ; 段基址（低16位）
    db 0x00      ; 段基址（中8位）
    db 10010010b ; 存在、特权级0、数据段、可写
    db 11001111b ; 粒度4KB、32位保护模式、段限长度（高4位）
    db 0x00      ; 段基址（高8位）

gdt_desc:
    dw gdt_desc - gdt - 1 ; GDT长度
    dd gdt               ; GDT地址

; 消息
boot_msg db 'Booting ZZQ OS...', 0
error_msg db 'Error loading kernel!', 0

; 填充剩余空间并添加启动签名
times 510-($-$$) db 0
dw 0xaa55
