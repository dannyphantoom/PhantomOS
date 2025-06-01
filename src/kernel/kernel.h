#ifndef KERNEL_H
#define KERNEL_H

// VGA color constants
typedef enum {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
} vga_color;

// Basic types
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

#define NULL ((void*)0)

// Terminal functions
void terminal_clear(void);
void terminal_setcolor(uint8_t color);
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void terminal_writestring(const char* data);
void terminal_putchar(char c);
uint8_t vga_entry_color(vga_color fg, vga_color bg);

// String functions
size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
void strcpy(char* dest, const char* src);
int strncmp(const char* str1, const char* str2, size_t n);
void strncpy(char* dest, const char* src, size_t n);

// Memory functions
void* kmalloc(size_t size);
void kfree(void* ptr);
void* memset(void* ptr, int value, size_t size);
void* memcpy(void* dest, const void* src, size_t size);

// Shell functions
void shell_prompt(void);

// Basic math
uint32_t get_current_time(void); // Simple time counter

#endif // KERNEL_H 