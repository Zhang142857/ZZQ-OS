[bits 16]
; 切换到32位保护模式
switch_to_pm:
    cli                     ; 关闭中断
    lgdt [gdt_descriptor]   ; 加载GDT
    
    mov eax, cr0            ; 打开cr0的第一位，切换到保护模式
    or eax, 0x1
    mov cr0, eax
    
    jmp CODE_SEG:init_pm    ; 远跳转到32位代码，强制刷新CPU流水线

[bits 32]
; 初始化保护模式下的寄存器和栈
init_pm:
    mov ax, DATA_SEG        ; 将数据段选择子设为所有数据段寄存器
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov ebp, 0x90000        ; 更新栈位置，使其在自由空间的顶部
    mov esp, ebp
    
    call BEGIN_PM           ; 调用在保护模式下开始的代码 