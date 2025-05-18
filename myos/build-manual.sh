#!/bin/bash

# 清理之前的构建文件
rm -f kernel/*.o kernel/*.elf

# 编译内核入口
nasm -f elf32 -o kernel/kernel_entry.o kernel/boot.asm

# 修复VERSION宏问题的编译命令
sed -i 's/print_string("About ZZQ OS " VERSION "\\n");/print_string("About ZZQ OS V1.0\\n");/g' kernel/simple_kernel.c
sed -i 's/print_string("Version: " VERSION "\\n");/print_string("Version: V1.0\\n");/g' kernel/simple_kernel.c
sed -i 's/print_string("Welcome to ZZQ OS VERSION " VERSION "!\\n");/print_string("Welcome to ZZQ OS VERSION V1.0!\\n");/g' kernel/simple_kernel.c

# 编译简化的内核源文件
gcc -ffreestanding -fno-pie -m32 -c kernel/simple_memory.c -o kernel/memory.o || { echo "编译内存管理模块失败"; exit 1; }
gcc -ffreestanding -fno-pie -m32 -c kernel/simple_timer.c -o kernel/timer.o || { echo "编译定时器模块失败"; exit 1; }
gcc -ffreestanding -fno-pie -m32 -c kernel/simple_kernel.c -o kernel/kernel.o || { echo "编译内核模块失败"; exit 1; }

# 编译string.c
gcc -ffreestanding -fno-pie -m32 -c kernel/string.c -o kernel/string.o || { echo "编译字符串模块失败"; exit 1; }

# 编译ntfs.c
gcc -ffreestanding -fno-pie -m32 -c kernel/ntfs.c -o kernel/ntfs.o || { echo "编译NTFS模块失败"; exit 1; }

# 链接内核 - 保留ELF格式供GRUB使用
ld -m elf_i386 -o kernel/kernel.elf -T kernel/linker.ld kernel/kernel_entry.o kernel/kernel.o kernel/memory.o kernel/timer.o kernel/string.o kernel/ntfs.o || { echo "链接内核失败"; exit 1; }

# 创建ISO镜像
echo "创建ISO镜像..."
mkdir -p iso_tmp/boot/grub
cp kernel/kernel.elf iso_tmp/boot/kernel.elf
echo "menuentry \"ZZQ-OS V1.0 with NTFS\" {" > iso_tmp/boot/grub/grub.cfg
echo "  multiboot /boot/kernel.elf" >> iso_tmp/boot/grub/grub.cfg
echo "  boot" >> iso_tmp/boot/grub/grub.cfg
echo "}" >> iso_tmp/boot/grub/grub.cfg

grub-mkrescue -o zzqos_V1.0.iso iso_tmp || { echo "创建ISO镜像失败"; exit 1; }

# 清理临时文件
rm -rf iso_tmp

echo "成功创建ISO镜像: zzqos_V1.0.iso" 