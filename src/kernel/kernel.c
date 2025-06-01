// PhantomOS Kernel
// Basic kernel implementation with VGA text mode output, keyboard input, and file system

#include "kernel.h"
#include "filesystem.h"
#include "editor.h"

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

// IDT entry structure (32-bit)
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

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
void tree_print_node(fs_node_t* node, int depth, int is_last);
void run_editor(const char* filename);

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

// Editor state
static int editor_active = 0;
static editor_state_t* current_editor = NULL;

// Keyboard state
static int shift_pressed = 0;
static int caps_lock = 0;
static int use_german_layout = 1; // Default to German layout

// US QWERTY scancode to ASCII translation table - normal (unshifted)
static char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '
};

// US QWERTY scancode to ASCII translation table - shifted
static char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' '
};

// German QWERTZ keyboard layout - normal (unshifted)
static char scancode_to_ascii_de[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 's', '\'', 0, 0,
    'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 'u', '+', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'o', 'a', '^', 0, '#',
    'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0, 0, 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '<', 0, 0, 0, 0
};

// German QWERTZ keyboard layout - shifted
static char scancode_to_ascii_de_shift[] = {
    0, 0, '!', '"', '#', '$', '%', '&', '/', '(', ')', '=', '?', '`', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I', 'O', 'P', 'U', '*', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'O', 'A', '^', 0, '\'',
    'Y', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_', 0, 0, 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '>', 0, 0, 0, 0
};

// Note: Due to ASCII limitations:
// - German umlauts (ä,ö,ü) are shown as a,o,u  
// - ß (eszett) is shown as s
// - § (section sign) is shown as #

// Shift key scancodes
#define SCANCODE_LEFT_SHIFT 0x2A
#define SCANCODE_RIGHT_SHIFT 0x36
#define SCANCODE_CAPS_LOCK 0x3A

// I/O port functions
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void) {
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

// Enable PS/2 keyboard and start scanning
static void keyboard_init(void) {
    // Wait for input buffer to be clear
    while (inb(KEYBOARD_STATUS_PORT) & 0x02)
        io_wait();
    
    // Disable devices
    outb(KEYBOARD_STATUS_PORT, 0xAD);  // Disable first PS/2 port
    io_wait();
    outb(KEYBOARD_STATUS_PORT, 0xA7);  // Disable second PS/2 port (if exists)
    io_wait();
    
    // Flush output buffer
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
        io_wait();
    }
    
    // Set controller configuration byte
    outb(KEYBOARD_STATUS_PORT, 0x20);  // Read configuration byte
    io_wait();
    uint8_t config = inb(KEYBOARD_DATA_PORT);
    config |= 0x01;  // Enable IRQ1
    config &= ~0x10; // Enable clock for first PS/2 port
    
    outb(KEYBOARD_STATUS_PORT, 0x60);  // Write configuration byte
    io_wait();
    outb(KEYBOARD_DATA_PORT, config);
    io_wait();
    
    // Enable first PS/2 port
    outb(KEYBOARD_STATUS_PORT, 0xAE);
    io_wait();
    
    // Reset keyboard
    outb(KEYBOARD_DATA_PORT, 0xFF);
    io_wait();
    
    // Wait for ACK and self-test result
    while (!(inb(KEYBOARD_STATUS_PORT) & 0x01))
        io_wait();
    inb(KEYBOARD_DATA_PORT);  // Read ACK (0xFA)
    
    while (!(inb(KEYBOARD_STATUS_PORT) & 0x01))
        io_wait();
    inb(KEYBOARD_DATA_PORT);  // Read self-test result (0xAA)
    
    // Enable scanning
    outb(KEYBOARD_DATA_PORT, 0xF4);
    io_wait();
    
    // Wait for ACK
    while (!(inb(KEYBOARD_STATUS_PORT) & 0x01))
        io_wait();
    inb(KEYBOARD_DATA_PORT);  // Read ACK
}

// Helper function to create VGA entry
uint8_t vga_entry_color(vga_color fg, vga_color bg) {
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
    int key_released = scancode & 0x80;
    scancode &= 0x7F; // Get the actual scancode without release bit
    
    // Track shift key state
    if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT) {
        shift_pressed = !key_released;
        outb(0x20, 0x20);  // End of interrupt to PIC
        return;
    }
    
    // Track caps lock (toggle on press)
    if (scancode == SCANCODE_CAPS_LOCK && !key_released) {
        caps_lock = !caps_lock;
        outb(0x20, 0x20);  // End of interrupt to PIC
        return;
    }

    // If editor is active, redirect input to editor
    if (editor_active && current_editor) {
        // Only process key press events (ignore releases)
        if (!key_released) {
            char ascii = 0;
            if (scancode < sizeof(scancode_to_ascii_de)) {
                // Use selected keyboard layout
                if (use_german_layout) {
                    ascii = shift_pressed ? scancode_to_ascii_de_shift[scancode] : scancode_to_ascii_de[scancode];
                } else {
                    ascii = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
                }
                
                // Apply caps lock for letters
                if (caps_lock && ascii >= 'a' && ascii <= 'z') {
                    ascii = ascii - 'a' + 'A';
                }
            }
            
            // Pass both ASCII character and raw scancode to editor
            editor_process_key(current_editor, ascii, scancode);
            
            // Force redraw after each key
            editor_draw(current_editor);
            
            // Check if editor wants to exit
            if (current_editor->mode == -1) {
                editor_active = 0;
                current_editor = NULL;
                terminal_clear();
                shell_prompt();
            }
        }
        
        outb(0x20, 0x20);  // End of interrupt to PIC
        return;
    }

    // Normal shell input processing
    if (!key_released) {  // Key press only (ignore key release)
        if (scancode < sizeof(scancode_to_ascii_de)) {
            char ascii;
            // Use selected keyboard layout
            if (use_german_layout) {
                ascii = shift_pressed ? scancode_to_ascii_de_shift[scancode] : scancode_to_ascii_de[scancode];
            } else {
                ascii = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
            }
            
            // Apply caps lock for letters
            if (caps_lock && ascii >= 'a' && ascii <= 'z') {
                ascii = ascii - 'a' + 'A';
            }
            
            if (ascii != 0) {
                if (ascii == '\n') {
                    terminal_putchar('\n');
                    input_buffer[input_length] = '\0';
                    process_command(input_buffer);
                    input_length = 0;
                    shell_prompt();
                } else if (input_length < sizeof(input_buffer) - 1) {
                    input_buffer[input_length++] = ascii;
                    terminal_putchar(ascii);
                }
            }
        } else if (scancode == 0x0E) { // Backspace scancode
            if (input_length > 0) {
                input_length--;
                terminal_putchar('\b');
            }
        }
    }

    outb(0x20, 0x20);  // End of interrupt to PIC
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
        terminal_writestring("PhantomOS Shell Commands (POSIX-compatible):\n\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("Basic Commands:\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("  help         - Show this help message\n");
        terminal_writestring("  clear        - Clear the screen\n");
        terminal_writestring("  echo <text>  - Echo text to the screen\n");
        terminal_writestring("  version      - Show OS version\n");
        terminal_writestring("  exit         - Halt the system\n\n");
        
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("File System Commands:\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("  pwd          - Print working directory\n");
        terminal_writestring("  ls [dir]     - List directory contents\n");
        terminal_writestring("  cd <dir>     - Change directory\n");
        terminal_writestring("  mkdir <dir>  - Make directory\n");
        terminal_writestring("  rmdir <dir>  - Remove empty directory\n");
        terminal_writestring("  stat <file>  - Show file information\n\n");
        
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("File Operations:\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("  touch <file> - Create empty file\n");
        terminal_writestring("  rm <file>    - Remove file\n");
        terminal_writestring("  cp <s> <d>   - Copy file\n");
        terminal_writestring("  mv <s> <d>   - Move/rename file\n");
        terminal_writestring("  cat <file>   - Display file contents\n");
        terminal_writestring("  write <file> <text> - Write text to file\n");
        terminal_writestring("  stat <file>  - Show file information\n");
        terminal_writestring("  tree [dir]   - Show directory tree\n");
        terminal_writestring("  edit <file>  - Edit file in text editor\n");
        terminal_writestring("  vi <file>    - Edit file (alias for edit)\n");
        terminal_writestring("  kbd <layout> - Set keyboard layout (de/us)\n");
        
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
        
    } else if (strcmp(cmd, "version") == 0) {
        terminal_writestring("PhantomOS v0.4 - 32-bit Kernel with POSIX File System\n");
        terminal_writestring("Features: German/US keyboard layouts, uppercase support, vim-like editor\n");
        
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
        
    } else if (strcmp(cmd, "ls") == 0) {
        fs_node_t* dir;
        if (strlen(arg1) > 0) {
            dir = fs_resolve_path(arg1);
            if (!dir || dir->type != FILE_TYPE_DIRECTORY) {
                terminal_writestring("ls: cannot access '");
                terminal_writestring(arg1);
                terminal_writestring("': No such directory\n");
                return;
            }
        } else {
            dir = fs_get_current_dir();
        }
        
        // List directory contents
        for (size_t i = 0; i < dir->child_count; i++) {
            fs_node_t* child = dir->children[i];
            if (child->type == FILE_TYPE_DIRECTORY) {
                terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
                terminal_writestring(child->name);
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            } else {
                terminal_writestring(child->name);
            }
            terminal_writestring(" ");
        }
        if (dir->child_count > 0) {
            terminal_writestring("\n");
        }
        
    } else if (strcmp(cmd, "cd") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("cd: missing operand\n");
        } else {
            if (strcmp(arg1, "/") == 0) {
                // Go to root
                fs_change_directory("/");
            } else if (strcmp(arg1, "..") == 0) {
                // Go to parent directory
                fs_node_t* current = fs_get_current_dir();
                if (current->parent) {
                    fs_change_directory("..");
                }
            } else if (strcmp(arg1, ".") == 0) {
                // Stay in current directory - do nothing
            } else {
                if (fs_change_directory(arg1) != 0) {
                    terminal_writestring("cd: ");
                    terminal_writestring(arg1);
                    terminal_writestring(": No such directory\n");
                }
            }
        }
        
    } else if (strcmp(cmd, "mkdir") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("mkdir: missing operand\n");
        } else {
            fs_node_t* parent = fs_get_current_dir();
            fs_node_t* existing = fs_find_child(parent, arg1);
            if (existing) {
                terminal_writestring("mkdir: cannot create directory '");
                terminal_writestring(arg1);
                terminal_writestring("': File exists\n");
            } else {
                fs_node_t* new_dir = fs_create_file(arg1, FILE_TYPE_DIRECTORY);
                if (new_dir && fs_add_child(parent, new_dir) == 0) {
                    // Success - no output
                } else {
                    terminal_writestring("mkdir: cannot create directory\n");
                }
            }
        }
        
    } else if (strcmp(cmd, "rmdir") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("rmdir: missing operand\n");
        } else {
            fs_node_t* node = fs_resolve_path(arg1);
            if (!node) {
                terminal_writestring("rmdir: failed to remove '");
                terminal_writestring(arg1);
                terminal_writestring("': No such file or directory\n");
            } else if (node->type != FILE_TYPE_DIRECTORY) {
                terminal_writestring("rmdir: failed to remove '");
                terminal_writestring(arg1);
                terminal_writestring("': Not a directory\n");
            } else if (node->child_count > 0) {
                terminal_writestring("rmdir: failed to remove '");
                terminal_writestring(arg1);
                terminal_writestring("': Directory not empty\n");
            } else {
                fs_remove_child(node->parent, node->name);
                fs_delete_node(node);
            }
        }
        
    } else if (strcmp(cmd, "touch") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("touch: missing file operand\n");
        } else {
            fs_node_t* parent = fs_get_current_dir();
            fs_node_t* existing = fs_find_child(parent, arg1);
            if (!existing) {
                fs_node_t* new_file = fs_create_file(arg1, FILE_TYPE_REGULAR);
                if (new_file && fs_add_child(parent, new_file) == 0) {
                    // Success - no output
                } else {
                    terminal_writestring("touch: cannot create file\n");
                }
            }
            // If file exists, touch just updates timestamp (not implemented)
        }
        
    } else if (strcmp(cmd, "rm") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("rm: missing operand\n");
        } else {
            fs_node_t* node = fs_resolve_path(arg1);
            if (!node) {
                terminal_writestring("rm: cannot remove '");
                terminal_writestring(arg1);
                terminal_writestring("': No such file or directory\n");
            } else if (node->type == FILE_TYPE_DIRECTORY) {
                terminal_writestring("rm: cannot remove '");
                terminal_writestring(arg1);
                terminal_writestring("': Is a directory\n");
            } else {
                fs_remove_child(node->parent, node->name);
                fs_delete_node(node);
            }
        }
        
    } else if (strcmp(cmd, "cp") == 0) {
        if (strlen(arg1) == 0 || strlen(arg2) == 0) {
            terminal_writestring("cp: missing file operand\n");
        } else {
            if (fs_copy_file(arg1, arg2) != 0) {
                terminal_writestring("cp: cannot copy file\n");
            }
        }
        
    } else if (strcmp(cmd, "mv") == 0) {
        if (strlen(arg1) == 0 || strlen(arg2) == 0) {
            terminal_writestring("mv: missing file operand\n");
        } else {
            if (fs_move_file(arg1, arg2) != 0) {
                terminal_writestring("mv: cannot move file\n");
            }
        }
        
    } else if (strcmp(cmd, "cat") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("cat: missing file operand\n");
        } else {
            fs_node_t* node = fs_resolve_path(arg1);
            if (!node) {
                terminal_writestring("cat: ");
                terminal_writestring(arg1);
                terminal_writestring(": No such file or directory\n");
            } else if (node->type == FILE_TYPE_DIRECTORY) {
                terminal_writestring("cat: ");
                terminal_writestring(arg1);
                terminal_writestring(": Is a directory\n");
            } else {
                char* content = fs_read_file(node);
                if (content) {
                    terminal_writestring(content);
                    if (strlen(content) == 0 || content[strlen(content)-1] != '\n') {
                        terminal_writestring("\n");
                    }
                } else {
                    // Empty file
                }
            }
        }
        
    } else if (strcmp(cmd, "write") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("write: missing file operand\n");
        } else if (strlen(arg2) == 0) {
            terminal_writestring("write: missing text operand\n");
        } else {
            fs_node_t* node = fs_resolve_path(arg1);
            if (!node) {
                // Create the file if it doesn't exist
                fs_node_t* parent = fs_get_current_dir();
                node = fs_create_file(arg1, FILE_TYPE_REGULAR);
                if (node && fs_add_child(parent, node) != 0) {
                    terminal_writestring("write: cannot create file\n");
                    fs_delete_node(node);
                    node = NULL;
                }
            }
            
            if (node) {
                if (node->type == FILE_TYPE_DIRECTORY) {
                    terminal_writestring("write: ");
                    terminal_writestring(arg1);
                    terminal_writestring(": Is a directory\n");
                } else {
                    if (fs_write_file(node, arg2, strlen(arg2)) == 0) {
                        // Success - no output
                    } else {
                        terminal_writestring("write: cannot write to file\n");
                    }
                }
            }
        }
        
    } else if (strcmp(cmd, "stat") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("stat: missing file operand\n");
        } else {
            fs_node_t* node = fs_resolve_path(arg1);
            if (!node) {
                terminal_writestring("stat: cannot stat '");
                terminal_writestring(arg1);
                terminal_writestring("': No such file or directory\n");
            } else {
                terminal_writestring("  File: ");
                terminal_writestring(node->name);
                terminal_writestring("\n");
                terminal_writestring("  Type: ");
                if (node->type == FILE_TYPE_DIRECTORY) {
                    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
                    terminal_writestring("directory");
                } else {
                    terminal_writestring("regular file");
                }
                terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
                terminal_writestring("\n");
                
                if (node->type == FILE_TYPE_REGULAR) {
                    terminal_writestring("  Size: ");
                    // Convert size to string (simple implementation)
                    char size_str[32];
                    size_t size = node->size;
                    int i = 0;
                    if (size == 0) {
                        size_str[i++] = '0';
                    } else {
                        char temp[32];
                        int j = 0;
                        while (size > 0) {
                            temp[j++] = '0' + (size % 10);
                            size /= 10;
                        }
                        while (j > 0) {
                            size_str[i++] = temp[--j];
                        }
                    }
                    size_str[i] = '\0';
                    terminal_writestring(size_str);
                    terminal_writestring(" bytes\n");
                } else if (node->type == FILE_TYPE_DIRECTORY) {
                    terminal_writestring("  Contents: ");
                    char count_str[32];
                    size_t count = node->child_count;
                    int i = 0;
                    if (count == 0) {
                        count_str[i++] = '0';
                    } else {
                        char temp[32];
                        int j = 0;
                        while (count > 0) {
                            temp[j++] = '0' + (count % 10);
                            count /= 10;
                        }
                        while (j > 0) {
                            count_str[i++] = temp[--j];
                        }
                    }
                    count_str[i] = '\0';
                    terminal_writestring(count_str);
                    terminal_writestring(" items\n");
                }
            }
        }
        
    } else if (strcmp(cmd, "tree") == 0) {
        fs_node_t* dir;
        if (strlen(arg1) > 0) {
            dir = fs_resolve_path(arg1);
            if (!dir || dir->type != FILE_TYPE_DIRECTORY) {
                terminal_writestring("tree: ");
                terminal_writestring(arg1);
                terminal_writestring(": Not a directory\n");
                return;
            }
        } else {
            dir = fs_get_current_dir();
        }
        
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        terminal_writestring(dir->name);
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("\n");
        
        for (size_t i = 0; i < dir->child_count; i++) {
            tree_print_node(dir->children[i], 0, i == dir->child_count - 1);
        }
        
    } else if (strcmp(cmd, "edit") == 0 || strcmp(cmd, "vi") == 0) {
        if (strlen(arg1) == 0) {
            // Open editor with no file
            run_editor("");
        } else {
            // Open editor with specified file
            run_editor(arg1);
        }
        
    } else if (strcmp(cmd, "kbd") == 0) {
        if (strlen(arg1) == 0) {
            terminal_writestring("Current keyboard layout: ");
            terminal_writestring(use_german_layout ? "German (QWERTZ)" : "US (QWERTY)");
            terminal_writestring("\n");
            terminal_writestring("Usage: kbd <de|us>\n");
        } else if (strcmp(arg1, "de") == 0) {
            use_german_layout = 1;
            terminal_writestring("Keyboard layout switched to German (QWERTZ)\n");
        } else if (strcmp(arg1, "us") == 0) {
            use_german_layout = 0;
            terminal_writestring("Keyboard layout switched to US (QWERTY)\n");
        } else {
            terminal_writestring("kbd: invalid layout '");
            terminal_writestring(arg1);
            terminal_writestring("'. Use 'de' or 'us'\n");
        }
        
    } else {
        terminal_writestring("bash: ");
        terminal_writestring(cmd);
        terminal_writestring(": command not found\n");
    }
}

// Helper function to print tree structure
void tree_print_node(fs_node_t* node, int depth, int is_last) {
    // Print indentation
    for (int i = 0; i < depth; i++) {
        terminal_writestring("  ");
    }
    
    // Print tree branch
    if (depth > 0) {
        terminal_writestring(is_last ? "`-- " : "|-- ");
    }
    
    // Print node name with color
    if (node->type == FILE_TYPE_DIRECTORY) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        terminal_writestring(node->name);
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    } else {
        terminal_writestring(node->name);
    }
    terminal_writestring("\n");
    
    // Recursively print children for directories
    if (node->type == FILE_TYPE_DIRECTORY) {
        for (size_t i = 0; i < node->child_count; i++) {
            tree_print_node(node->children[i], depth + 1, i == node->child_count - 1);
        }
    }
}

void init_idt(void) {
    // Remap PIC
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Mask all IRQs then unmask keyboard (IRQ1)
    outb(0x21, 0xFF);  // mask all on master PIC
    outb(0xA1, 0xFF);  // mask all on slave PIC
    outb(0x21, 0xFD);  // enable only keyboard (bit 1 cleared)

    // ✅ Now install IDT entry AFTER remapping
    idt_set_entry(0x20, (uint32_t)keyboard_interrupt_handler, KERNEL_CODE_SEGMENT_OFFSET, 0x8E); // Timer IRQ0
    idt_set_entry(0x21, (uint32_t)keyboard_interrupt_handler, KERNEL_CODE_SEGMENT_OFFSET, 0x8E); // Keyboard IRQ1
    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base = (uint32_t)&idt;
    asm volatile ("lidt %0" : : "m"(idt_pointer));

    uint8_t m = inb(0x21);
    char* vga = (char*)0xB8000;
    vga[5] = 0x0F;

    // Enable interrupts
    asm volatile ("sti");

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
    char* video = (char*)0xB8000;
  


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
    terminal_writestring("  - POSIX-compatible shell commands\n");
    terminal_writestring("  - Vim-like text editor\n");
    terminal_writestring("  - German/US keyboard layouts (type 'kbd' for info)\n\n");
    
    // Initialize file system
    fs_init();

    // Enable interrupts first, then keyboard
    init_idt();
    keyboard_init();
    
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

// Wrapper function to run the editor
void run_editor(const char* filename) {
    static editor_state_t editor;
    current_editor = &editor;
    editor_active = 1;
    
    editor_init(current_editor);
    
    if (filename && strlen(filename) > 0) {
        strcpy(current_editor->filename, filename);
        editor_open(current_editor, filename);
    } else {
        // Set default filename
        strcpy(current_editor->filename, "untitled.txt");
    }
    
    // Initial draw
    editor_draw(current_editor);
} 
