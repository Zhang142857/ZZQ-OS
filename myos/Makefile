CC = gcc
ASM = nasm
LD = ld

# 目标文件
BOOT_BIN = boot/boot.bin
KERNEL_ENTRY = kernel/kernel_entry.o
KERNEL_BIN = kernel/kernel.bin
KERNEL_OBJ = kernel/kernel.o
MEMORY_OBJ = kernel/memory.o
TIMER_OBJ = kernel/timer.o
FS_OBJ = kernel/fs.o
STRING_OBJ = kernel/string.o
DISK_OBJ = kernel/disk.o
FAT32_OBJ = kernel/fat32.o
FAT32_IO_OBJ = kernel/fat32_io.o
NTFS_OBJ = kernel/ntfs.o
OS_IMG = zzqos.img
DATA_IMG = diskdata.img
ISO_FILE = zzqos.iso

# 编译标志
ASM_FLAGS = -f bin
ASM_ELF_FLAGS = -f elf
C_FLAGS = -ffreestanding -fno-pie -m32 -c
LD_FLAGS = -m elf_i386 -T kernel/linker.ld --oformat binary -nostdlib

all: $(OS_IMG) $(DATA_IMG)

# 编译引导加载程序
$(BOOT_BIN): boot/boot.asm boot/print_string.inc boot/disk_load.inc boot/gdt.inc boot/print_string_pm.inc boot/switch_to_pm.inc
	$(ASM) $(ASM_FLAGS) $< -o $@

# 编译内核入口点
$(KERNEL_ENTRY): kernel/boot.asm
	$(ASM) $(ASM_ELF_FLAGS) $< -o $@

# 编译C内核
$(KERNEL_OBJ): kernel/kernel.c kernel/memory.h kernel/timer.h kernel/fs.h kernel/string.h kernel/disk.h kernel/fat32.h kernel/ntfs.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译内存管理模块
$(MEMORY_OBJ): kernel/memory.c kernel/memory.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译定时器模块
$(TIMER_OBJ): kernel/timer.c kernel/timer.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译字符串模块
$(STRING_OBJ): kernel/string.c kernel/string.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译文件系统模块
$(FS_OBJ): kernel/fs.c kernel/fs.h kernel/string.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译磁盘驱动模块
$(DISK_OBJ): kernel/disk.c kernel/disk.h kernel/string.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译FAT32文件系统模块
$(FAT32_OBJ): kernel/fat32.c kernel/fat32.h kernel/disk.h kernel/string.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译FAT32 I/O模块
$(FAT32_IO_OBJ): kernel/fat32_io.c kernel/fat32.h kernel/disk.h kernel/string.h
	$(CC) $(C_FLAGS) $< -o $@

# 编译NTFS文件系统模块
$(NTFS_OBJ): kernel/ntfs.c kernel/ntfs.h kernel/string.h
	$(CC) $(C_FLAGS) $< -o $@

# 链接内核
$(KERNEL_BIN): $(KERNEL_ENTRY) $(KERNEL_OBJ) $(MEMORY_OBJ) $(TIMER_OBJ) $(FS_OBJ) $(STRING_OBJ) $(DISK_OBJ) $(FAT32_OBJ) $(FAT32_IO_OBJ) $(NTFS_OBJ)
	$(LD) $(LD_FLAGS) $^ -o $@

# 创建磁盘镜像
$(OS_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	dd if=/dev/zero of=$(OS_IMG) bs=512 count=2880
	dd if=$(BOOT_BIN) of=$(OS_IMG) conv=notrunc
	dd if=$(KERNEL_BIN) of=$(OS_IMG) seek=1 conv=notrunc

# 创建数据磁盘镜像（如果不存在）
$(DATA_IMG):
	@if [ ! -f $(DATA_IMG) ]; then \
		echo "Creating new data disk image..."; \
		dd if=/dev/zero of=$(DATA_IMG) bs=512 count=2880; \
	else \
		echo "Using existing data disk image."; \
	fi

# 创建ISO镜像
$(ISO_FILE): $(OS_IMG)
	@mkdir -p iso_tmp/boot/grub
	@cp $(OS_IMG) iso_tmp/boot/
	@echo "menuentry \"ZZQ-OS\" {" > iso_tmp/boot/grub/grub.cfg
	@echo "  multiboot /boot/$(OS_IMG)" >> iso_tmp/boot/grub/grub.cfg
	@echo "}" >> iso_tmp/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO_FILE) iso_tmp
	@rm -rf iso_tmp

# 使用QEMU运行 (使用VNC)
run: $(OS_IMG) $(DATA_IMG)
	qemu-system-i386 -drive file=$(OS_IMG),format=raw,if=floppy -drive file=$(DATA_IMG),format=raw,if=floppy,index=1 -display vnc=0.0.0.0:0

# 使用QEMU运行 (使用SDL显示)
run-sdl: $(OS_IMG) $(DATA_IMG)
	qemu-system-i386 -drive file=$(OS_IMG),format=raw,if=floppy -drive file=$(DATA_IMG),format=raw,if=floppy,index=1 -display sdl

# 使用QEMU运行 (使用curses终端显示)
run-curses: $(OS_IMG) $(DATA_IMG)
	qemu-system-i386 -drive file=$(OS_IMG),format=raw,if=floppy -drive file=$(DATA_IMG),format=raw,if=floppy,index=1 -display curses

# 使用QEMU运行ISO镜像
run-iso: $(ISO_FILE)
	qemu-system-i386 -cdrom $(ISO_FILE) -display vnc=0.0.0.0:0

# 使用QEMU运行并启用调试
debug: $(OS_IMG) $(DATA_IMG)
	qemu-system-i386 -drive file=$(OS_IMG),format=raw,if=floppy -drive file=$(DATA_IMG),format=raw,if=floppy,index=1 -display vnc=0.0.0.0:0 -s -S

# 清理
clean:
	rm -f $(BOOT_BIN) $(KERNEL_BIN) $(KERNEL_ENTRY) $(KERNEL_OBJ) $(MEMORY_OBJ) $(TIMER_OBJ) $(FS_OBJ) $(STRING_OBJ) $(DISK_OBJ) $(FAT32_OBJ) $(FAT32_IO_OBJ) $(NTFS_OBJ) $(OS_IMG)

# 完全清理（包括数据磁盘和ISO）
clean-all: clean
	rm -f $(DATA_IMG) $(ISO_FILE)

# 简化的ISO目标，只包含基本功能和NTFS支持
simple-iso: boot/boot.bin kernel/kernel_entry.o kernel/simple_kernel.o kernel/memory.o kernel/timer.o kernel/string.o kernel/ntfs.o
	$(LD) -m elf_i386 -T kernel/linker.ld --oformat binary -nostdlib kernel/kernel_entry.o kernel/simple_kernel.o kernel/memory.o kernel/timer.o kernel/string.o kernel/ntfs.o -o kernel/simple_kernel.bin
	dd if=/dev/zero of=$(OS_IMG) bs=512 count=2880
	dd if=boot/boot.bin of=$(OS_IMG) conv=notrunc
	dd if=kernel/simple_kernel.bin of=$(OS_IMG) seek=1 conv=notrunc
	@mkdir -p iso_tmp/boot/grub
	@cp $(OS_IMG) iso_tmp/boot/
	@echo "menuentry \"ZZQ-OS\" {" > iso_tmp/boot/grub/grub.cfg
	@echo "  multiboot /boot/$(OS_IMG)" >> iso_tmp/boot/grub/grub.cfg
	@echo "}" >> iso_tmp/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO_FILE) iso_tmp
	@rm -rf iso_tmp
	@echo "Simple ISO image created: $(ISO_FILE)"

.PHONY: all run run-sdl run-curses run-iso debug clean clean-all simple-iso 