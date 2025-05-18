#ifndef VGA_H
#define VGA_H

#include <stdint.h>

// VGA模式和参数
#define VGA_MODE_TEXT      0x03  // 文本模式 80x25
#define VGA_MODE_GRAPHIC   0x13  // 图形模式 320x200 256色

// 颜色定义
#define VGA_BLACK          0
#define VGA_BLUE           1
#define VGA_GREEN          2
#define VGA_CYAN           3
#define VGA_RED            4
#define VGA_MAGENTA        5
#define VGA_BROWN          6
#define VGA_LIGHT_GRAY     7
#define VGA_DARK_GRAY      8
#define VGA_LIGHT_BLUE     9
#define VGA_LIGHT_GREEN    10
#define VGA_LIGHT_CYAN     11
#define VGA_LIGHT_RED      12
#define VGA_LIGHT_MAGENTA  13
#define VGA_YELLOW         14
#define VGA_WHITE          15

// 图形函数
void vga_init();
void vga_set_mode(uint8_t mode);
void vga_clear_screen(uint8_t color);
void vga_plot_pixel(int x, int y, uint8_t color);
void vga_draw_line(int x1, int y1, int x2, int y2, uint8_t color);
void vga_draw_rectangle(int x, int y, int width, int height, uint8_t color);
void vga_draw_circle(int x, int y, int radius, uint8_t color);
void vga_fill_rectangle(int x, int y, int width, int height, uint8_t color);
void vga_draw_text(int x, int y, const char* text, uint8_t color);

#endif // VGA_H 