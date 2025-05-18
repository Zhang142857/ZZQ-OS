[bits 16]
; GDT
gdt_start:

gdt_null: ; 强制的空描述符
    dd 0x0 ; 'dd' 表示定义双字 (4 bytes)
    dd 0x0

gdt_code: ; 代码段描述符
    ; base=0x0, limit=0xfffff,
    ; 1st flags: (present)1 (privilege)00 (descriptor type)1 -> 1001b
    ; type flags: (code)1 (conforming)0 (readable)1 (accessed)0 -> 1010b
    ; 2nd flags: (granularity)1 (32-bit default)1 (64-bit seg)0 (AVL)0 -> 1100b
    dw 0xffff    ; Limit (bits 0-15)
    dw 0x0       ; Base (bits 0-15)
    db 0x0       ; Base (bits 16-23)
    db 10011010b ; 1st flags, type flags
    db 11001111b ; 2nd flags, Limit (bits 16-19)
    db 0x0       ; Base (bits 24-31)

gdt_data: ; 数据段描述符
    ; 与代码段相同，除了type flags:
    ; type flags: (code)0 (expand down)0 (writable)1 (accessed)0 -> 0010b
    dw 0xffff    ; Limit (bits 0-15)
    dw 0x0       ; Base (bits 0-15)
    db 0x0       ; Base (bits 16-23)
    db 10010010b ; 1st flags, type flags
    db 11001111b ; 2nd flags, Limit (bits 16-19)
    db 0x0       ; Base (bits 24-31)

gdt_end: ; 用于计算GDT大小

; GDT描述符
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; GDT大小 (总是比真实大小小1)
    dd gdt_start                ; GDT地址

; 常量
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start 