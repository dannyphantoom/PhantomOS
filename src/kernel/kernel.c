// PhantomOS Kernel
// Basic kernel implementation with VGA text mode output, keyboard input, and file system

#include "kernel.h"
#include "filesystem.h"

// Define basic types for freestanding environment (32-bit)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int size_t;

#define NULL ((void*)0)

// VGA text mode constants
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Keyboard constants
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

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

// IDT entry structure (32-bit)
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
};

// IDT pointer structure
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Forward declarations
void process_command(const char* command);
void shell_prompt(void);
int strncmp(const char* str1, const char* str2, size_t n);
void parse_command_args(const char* command, char* cmd, char* arg1, char* arg2);

// Global variables for terminal state
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

// Global variables for keyboard input
static char input_buffer[256];
static size_t input_length = 0;
static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr idt_pointer;

// US QWERTY scancode to ASCII translation table
static char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '
};

// I/O port functions
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Helper function to create VGA entry
static inline uint8_t vga_entry_color(vga_color fg, vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// String functions
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && *str2 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *str1 - *str2;
}

void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

int strncmp(const char* str1, const char* str2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (str1[i] != str2[i] || str1[i] == '\0' || str2[i] == '\0') {
            return str1[i] - str2[i];
        }
    }
    return 0;
}

// Initialize the terminal
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) VGA_MEMORY;
    
    // Clear the screen
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

// Set terminal color
void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

// Put character at specific position
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

// Scroll the terminal up by one line
void terminal_scroll(void) {
    // Move all lines up
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear the bottom line
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    
    terminal_row = VGA_HEIGHT - 1;
}

// Put a single character
void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
        return;
    }
    
    if (c == '\r') {
        terminal_column = 0;
        return;
    }
    
    if (c == '\t') {
        // Simple tab implementation - move to next multiple of 4
        terminal_column = (terminal_column + 4) & ~3;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                terminal_scroll();
            }
        }
        return;
    }
    
    if (c == '\b') { // Backspace
        if (terminal_column > 0) {
            terminal_column--;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
        return;
    }
    
    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    }
}

// Print a string
void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

// Print a null-terminated string
void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

// Clear the screen
void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
}

// External assembly function declaration for keyboard interrupt handler
extern void keyboard_interrupt_handler(void);

// Set up an IDT entry (32-bit version)
void idt_set_entry(int num, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = type_attr;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
}

// Keyboard interrupt handler (called from assembly)
void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Only handle key presses (not releases)
    if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_to_ascii) && scancode_to_ascii[scancode] != 0) {
            char ascii = scancode_to_ascii[scancode];
            
            if (ascii == '\n') {
                // Process command
                terminal_putchar('\n');
                input_buffer[input_length] = '\0';
                process_command(input_buffer);
                input_length = 0;
                shell_prompt();
            } else if (scancode == 0x0E) { // Backspace
                if (input_length > 0) {
                    input_length--;
                    terminal_putchar('\b');
                }
            } else if (input_length < sizeof(input_buffer) - 1) {
                input_buffer[input_length++] = ascii;
                terminal_putchar(ascii);
            }
        }
    }
    
    // Send End of Interrupt to PIC
    outb(0x20, 0x20);
}

// Parse command into command and arguments
void parse_command_args(const char* command, char* cmd, char* arg1, char* arg2) {
    size_t i = 0, cmd_len = 0, arg1_len = 0, arg2_len = 0;
    int state = 0; // 0=cmd, 1=space, 2=arg1, 3=space, 4=arg2
    
    // Initialize outputs
    cmd[0] = '\0';
    arg1[0] = '\0';
    arg2[0] = '\0';
    
    while (command[i] && i < 255) {
        char c = command[i];
        
        if (state == 0) { // Reading command
            if (c == ' ') {
                cmd[cmd_len] = '\0';
                state = 1;
            } else if (cmd_len < 63) {
                cmd[cmd_len++] = c;
            }
        } else if (state == 1) { // Skip spaces
            if (c != ' ') {
                arg1[arg1_len++] = c;
                state = 2;
            }
        } else if (state == 2) { // Reading arg1
            if (c == ' ') {
                arg1[arg1_len] = '\0';
                state = 3;
            } else if (arg1_len < 63) {
                arg1[arg1_len++] = c;
            }
        } else if (state == 3) { // Skip spaces
            if (c != ' ') {
                arg2[arg2_len++] = c;
                state = 4;
            }
        } else if (state == 4) { // Reading arg2
            if (c != ' ' && arg2_len < 63) {
                arg2[arg2_len++] = c;
            }
        }
        i++;
    }
    
    // Null terminate strings
    cmd[cmd_len] = '\0';
    arg1[arg1_len] = '\0';
    arg2[arg2_len] = '\0';
}

// Process shell commands
void process_command(const char* command) {
    if (strlen(command) == 0) {
        return;
    }
    
    char cmd[64], arg1[64], arg2[64];
    parse_command_args(command, cmd, arg1, arg2);
    
    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("PhantomOS Shell Commands (POSIX-compatible):\n");
        terminal_writestring("  help       - Show this help message\n");
        terminal_writestring("  clear      - Clear the screen\n");
        terminal_writestring("  echo <txt> - Echo text\n");
        terminal_writestring("  version    - Show OS version\n");
        terminal_writestring("  exit       - Halt the system\n");
        terminal_writestring("  pwd        - Print working directory\n");
        terminal_writestring("  ls [dir]   - List directory contents\n");
        terminal_writestring("  cd <dir>   - Change directory\n");
        terminal_writestring("  mkdir <dir>- Make directory\n");
        terminal_writestring("  rmdir <dir>- Remove empty directory\n");
        terminal_writestring("  touch <file>- Create file\n");
        terminal_writestring("  rm <file>  - Remove file\n");
        terminal_writestring("  cp <s> <d> - Copy file\n");
        terminal_writestring("  mv <s> <d> - Move/rename file\n");
        terminal_writestring("  cat <file> - Display file contents\n");
        
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
        
    } else if (strcmp(cmd, "version") == 0) {
        terminal_writestring("PhantomOS v0.3 - 32-bit Kernel with POSIX File System\n");
        terminal_writestring("Fixed bootloader, working keyboard input\n");
        
    } else if (strcmp(cmd, "exit") == 0) {
        terminal_writestring("Halting system...\n");
        asm volatile ("cli; hlt");
        
    } else if (strcmp(cmd, "pwd") == 0) {
        terminal_writestring(fs_get_current_path());
        terminal_writestring("\n");
        
    } else if (strcmp(cmd, "echo") == 0) {
        if (strlen(arg1) > 0) {
            terminal_writestring(arg1);
            if (strlen(arg2) > 0) {
                terminal_writestring(" ");
                terminal_writestring(arg2);
            }
        }
        terminal_writestring("\n");
        
    } else {
        terminal_writestring("bash: ");
        terminal_writestring(cmd);
        terminal_writestring(": command not found\n");
    }
}

// Initialize IDT
void init_idt(void) {
    // Set up keyboard interrupt (IRQ1 = INT 0x21)
    idt_set_entry(0x21, (uint32_t)keyboard_interrupt_handler, KERNEL_CODE_SEGMENT_OFFSET, 0x8E);
    
    // Set up IDT pointer
    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base = (uint32_t)&idt;
    
    // Load IDT
    asm volatile ("lidt %0" : : "m"(idt_pointer));
    
    // Enable interrupts
    asm volatile ("sti");
    
    // Initialize PIC (Programmable Interrupt Controller)
    // Remap IRQs to start at interrupt 0x20
    outb(0x20, 0x11); // Initialize PIC1
    outb(0xA0, 0x11); // Initialize PIC2
    outb(0x21, 0x20); // PIC1 offset (IRQ0-7 -> INT 0x20-0x27)
    outb(0xA1, 0x28); // PIC2 offset (IRQ8-15 -> INT 0x28-0x2F)
    outb(0x21, 0x04); // Tell PIC1 that PIC2 is at IRQ2
    outb(0xA1, 0x02); // Tell PIC2 its cascade identity
    outb(0x21, 0x01); // 8086 mode for PIC1
    outb(0xA1, 0x01); // 8086 mode for PIC2
    
    // Enable keyboard interrupt (IRQ1)
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 1); // Clear bit 1 (IRQ1)
    outb(0x21, mask);
}

// Basic shell prompt with current directory
void shell_prompt(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("phantom");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring(":");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    terminal_writestring(fs_get_current_path());
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("$ ");
}

// Main kernel entry point
void kernel_main(void) {
    // Initialize the terminal
    terminal_initialize();
    
    // Print welcome message
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("=== PhantomOS 32-bit Kernel ===\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("32-bit kernel with POSIX file system loaded!\n\n");
    
    terminal_writestring("Kernel initialized with:\n");
    terminal_writestring("  - VGA text mode output\n");
    terminal_writestring("  - Keyboard input handling\n");
    terminal_writestring("  - Interrupt system\n");
    terminal_writestring("  - In-memory file system\n");
    terminal_writestring("  - POSIX-compatible shell commands\n\n");
    
    // Initialize file system
    fs_init();
    
    // Initialize interrupt system
    init_idt();
    
    // Start the shell
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    terminal_writestring("Starting PhantomOS Shell...\n");
    terminal_writestring("Type 'help' for available commands.\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    shell_prompt();
    
    // Main kernel loop - just wait for interrupts
    while (1) {
        asm volatile ("hlt"); // Halt until next interrupt
    }
} 