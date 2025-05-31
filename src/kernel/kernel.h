#ifndef KERNEL_H
#define KERNEL_H

// Define basic types for freestanding environment (32-bit)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

#define NULL ((void*)0)

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

// Terminal functions
void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_setcolor(uint8_t color);

// Basic math
uint32_t get_current_time(void); // Simple time counter

#endif // KERNEL_H 