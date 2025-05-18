# ZZQ-OS

一个具有硬盘驱动和FAT32文件系统的简单X86操作系统。

## 功能特点

- 32位保护模式运行
- 内存管理系统
- 交互式命令行界面
- ATA/IDE硬盘驱动
- FAT32文件系统支持
- 文件和目录操作
- 分区管理

## 系统要求

构建和运行此操作系统需要以下工具：

- GCC 编译器
- NASM 汇编器
- GNU Make
- QEMU 模拟器（用于测试）
- xorriso 和 grub-mkrescue（用于创建ISO镜像）

## 构建说明

1. 克隆此仓库
2. 进入源代码目录
3. 执行以下命令：

```bash
make
```

这将创建系统镜像文件 `zzqos.img` 和 数据磁盘镜像 `diskdata.img`。

## 运行说明

### 使用QEMU运行

有几种运行方式：

1. 使用VNC显示（推荐用于远程调试）:

```bash
make run
```

2. 使用SDL显示（本地显示）:

```bash
make run-sdl
```

3. 使用终端文本显示:

```bash
make run-curses
```

### 使用ISO镜像

创建并运行ISO镜像:

```bash
make run-iso
```

## 使用说明

系统启动后，将显示命令提示符。以下是可用命令：

- `help` - 显示帮助信息
- `clear` - 清屏
- `sysinfo` - 显示系统信息
- `time` - 显示当前时间
- `meminfo` - 显示内存信息
- `disks` - 显示磁盘信息
- `format N` - 格式化分区N为FAT32
- `mount N` - 挂载分区N
- `ls` - 列出根目录文件
- `mkdir D` - 创建目录D
- `write F` - 创建文件F并写入数据
- `read F` - 显示文件F内容
- `fsinfo` - 显示文件系统信息

## 文件系统操作示例

1. 查看磁盘信息：
   ```
   disks
   ```

2. 格式化分区：
   ```
   format 0
   ```

3. 挂载分区：
   ```
   mount 0
   ```

4. 列出文件：
   ```
   ls
   ```

5. 创建目录：
   ```
   mkdir test
   ```

6. 创建文件：
   ```
   write hello.txt
   ```

7. 读取文件：
   ```
   read hello.txt
   ```

## 项目结构

- `/boot` - 引导加载程序
- `/kernel` - 内核代码
  - `kernel.c` - 主内核代码
  - `memory.c/.h` - 内存管理
  - `timer.c/.h` - 定时器功能
  - `fs.c/.h` - 基本文件系统接口
  - `disk.c/.h` - ATA/IDE硬盘驱动
  - `fat32.c/.h` - FAT32文件系统实现
  - `fat32_io.c` - FAT32文件I/O操作
  - `string.c/.h` - 字符串处理函数

## 清理

清理编译产物：

```bash
make clean
```

完全清理（包括磁盘镜像和ISO）：

```bash
make clean-all
``` 