// PhantomOS Text Editor - Vim-like implementation
#include "editor.h"
#include "kernel.h"
#include "filesystem.h"

// Special key scancodes
#define SCANCODE_ESC 0x01
#define SCANCODE_BACKSPACE 0x0E
#define SCANCODE_ENTER 0x1C
#define SCANCODE_UP 0x48
#define SCANCODE_DOWN 0x50
#define SCANCODE_LEFT 0x4B
#define SCANCODE_RIGHT 0x4D

// Initialize editor state
void editor_init(editor_state_t* editor) {
    editor->line_count = 1;
    editor->cursor_x = 0;
    editor->cursor_y = 0;
    editor->view_start_line = 0;
    editor->mode = MODE_NORMAL;
    editor->filename[0] = '\0';
    editor->modified = 0;
    editor->command_length = 0;
    editor->command_buffer[0] = '\0';
    editor_set_status(editor, "-- NORMAL --");
    
    // Initialize buffer with empty lines
    for (int i = 0; i < EDITOR_MAX_LINES; i++) {
        editor->buffer[i][0] = '\0';
    }
}

// Open a file in the editor
void editor_open(editor_state_t* editor, const char* filename) {
    strcpy(editor->filename, filename);
    
    // Try to read the file
    fs_node_t* node = fs_resolve_path(filename);
    if (node && node->type == FILE_TYPE_REGULAR) {
        char* content = fs_read_file(node);
        if (content) {
            // Parse content into lines
            int line = 0;
            int col = 0;
            
            for (int i = 0; content[i] != '\0' && line < EDITOR_MAX_LINES; i++) {
                if (content[i] == '\n') {
                    editor->buffer[line][col] = '\0';
                    line++;
                    col = 0;
                } else if (col < EDITOR_MAX_LINE_LENGTH - 1) {
                    editor->buffer[line][col] = content[i];
                    col++;
                }
            }
            
            if (col > 0) {
                editor->buffer[line][col] = '\0';
                line++;
            }
            
            editor->line_count = line > 0 ? line : 1;
        }
    }
}

// Set status message
void editor_set_status(editor_state_t* editor, const char* message) {
    strcpy(editor->status_message, message);
}

// Draw the editor screen
void editor_draw(editor_state_t* editor) {
    terminal_clear();
    
    // Draw text buffer (lines 0-22)
    for (int y = 0; y < 23 && y + editor->view_start_line < editor->line_count; y++) {
        int line_num = y + editor->view_start_line;
        
        // Draw line number in gray
        terminal_setcolor(vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
        
        // Simple number to string conversion
        char num_str[5];
        int num = line_num + 1;
        int i = 3;
        num_str[4] = '\0';
        
        for (int j = 0; j < 4; j++) {
            num_str[j] = ' ';
        }
        
        while (num > 0 && i >= 0) {
            num_str[i] = '0' + (num % 10);
            num /= 10;
            i--;
        }
        
        for (int j = 0; j < 4; j++) {
            terminal_putentryat(num_str[j], 
                vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK), j, y);
        }
        
        // Draw text content
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        char* line_text = editor->buffer[line_num];
        for (int x = 0; x < strlen(line_text) && x < 75; x++) {
            terminal_putentryat(line_text[x], 
                vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), x + 5, y);
        }
    }
    
    // Draw status line (line 23)
    terminal_setcolor(vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE));
    for (int x = 0; x < 80; x++) {
        terminal_putentryat(' ', vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE), x, 23);
    }
    
    // Show filename and mode
    char status[80];
    if (editor->modified) {
        status[0] = '[';
        status[1] = '+';
        status[2] = ']';
        status[3] = ' ';
        strcpy(&status[4], editor->filename[0] ? editor->filename : "[No Name]");
    } else {
        strcpy(status, editor->filename[0] ? editor->filename : "[No Name]");
    }
    
    for (int i = 0; i < strlen(status); i++) {
        terminal_putentryat(status[i], 
            vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE), i, 23);
    }
    
    // Show mode on right side
    const char* mode_str = editor->mode == MODE_INSERT ? "-- INSERT --" : 
                          editor->mode == MODE_COMMAND ? ":" : "-- NORMAL --";
    int mode_len = strlen(mode_str);
    for (int i = 0; i < mode_len; i++) {
        terminal_putentryat(mode_str[i], 
            vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE), 80 - mode_len + i, 23);
    }
    
    // Draw command line (line 24)
    if (editor->mode == MODE_COMMAND) {
        terminal_putentryat(':', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), 0, 24);
        for (int i = 0; i < editor->command_length; i++) {
            terminal_putentryat(editor->command_buffer[i], 
                vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), i + 1, 24);
        }
        // Show cursor in command line
        terminal_putentryat('_', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), 
            editor->command_length + 1, 24);
    } else {
        // Show status message
        for (int i = 0; i < strlen(editor->status_message); i++) {
            terminal_putentryat(editor->status_message[i], 
                vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), i, 24);
        }
    }
    
    // Position hardware cursor (blinking cursor)
    if (editor->mode != MODE_COMMAND) {
        int screen_y = editor->cursor_y - editor->view_start_line;
        if (screen_y >= 0 && screen_y < 23) {
            terminal_putentryat('_', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK), 
                editor->cursor_x + 5, screen_y);
        }
    }
}

// Move cursor with bounds checking
void editor_move_cursor(editor_state_t* editor, int dx, int dy) {
    editor->cursor_y += dy;
    editor->cursor_x += dx;
    
    // Vertical bounds
    if (editor->cursor_y < 0) {
        editor->cursor_y = 0;
    }
    if (editor->cursor_y >= editor->line_count) {
        editor->cursor_y = editor->line_count - 1;
    }
    
    // Horizontal bounds
    int line_len = strlen(editor->buffer[editor->cursor_y]);
    if (editor->cursor_x < 0) {
        editor->cursor_x = 0;
    }
    if (editor->cursor_x > line_len) {
        editor->cursor_x = line_len;
    }
    
    // Adjust view for scrolling
    if (editor->cursor_y < editor->view_start_line) {
        editor->view_start_line = editor->cursor_y;
    }
    if (editor->cursor_y >= editor->view_start_line + 23) {
        editor->view_start_line = editor->cursor_y - 22;
    }
}

// Insert a character at cursor position
void editor_insert_char(editor_state_t* editor, char c) {
    if (editor->cursor_y >= EDITOR_MAX_LINES) return;
    
    char* line = editor->buffer[editor->cursor_y];
    int len = strlen(line);
    
    if (c == '\n') {
        // Split line at cursor
        if (editor->line_count < EDITOR_MAX_LINES - 1) {
            // Shift lines down
            for (int i = editor->line_count; i > editor->cursor_y + 1; i--) {
                strcpy(editor->buffer[i], editor->buffer[i - 1]);
            }
            
            // Split current line
            strcpy(editor->buffer[editor->cursor_y + 1], &line[editor->cursor_x]);
            line[editor->cursor_x] = '\0';
            
            editor->line_count++;
            editor->cursor_y++;
            editor->cursor_x = 0;
            editor->modified = 1;
        }
    } else if (len < EDITOR_MAX_LINE_LENGTH - 1) {
        // Insert character
        for (int i = len; i >= editor->cursor_x; i--) {
            line[i + 1] = line[i];
        }
        line[editor->cursor_x] = c;
        editor->cursor_x++;
        editor->modified = 1;
    }
}

// Delete character before cursor
void editor_delete_char(editor_state_t* editor) {
    if (editor->cursor_x == 0 && editor->cursor_y == 0) return;
    
    if (editor->cursor_x == 0) {
        // Join with previous line
        if (editor->cursor_y > 0) {
            char* prev_line = editor->buffer[editor->cursor_y - 1];
            char* curr_line = editor->buffer[editor->cursor_y];
            int prev_len = strlen(prev_line);
            
            // Append current line to previous
            strcpy(&prev_line[prev_len], curr_line);
            
            // Shift lines up
            for (int i = editor->cursor_y; i < editor->line_count - 1; i++) {
                strcpy(editor->buffer[i], editor->buffer[i + 1]);
            }
            
            editor->line_count--;
            editor->cursor_y--;
            editor->cursor_x = prev_len;
            editor->modified = 1;
        }
    } else {
        // Delete character in current line
        char* line = editor->buffer[editor->cursor_y];
        int len = strlen(line);
        
        for (int i = editor->cursor_x - 1; i < len; i++) {
            line[i] = line[i + 1];
        }
        
        editor->cursor_x--;
        editor->modified = 1;
    }
}

// Save file
void editor_save_file(editor_state_t* editor) {
    // Build file content
    char content[EDITOR_MAX_LINES * EDITOR_MAX_LINE_LENGTH];
    int pos = 0;
    
    for (int i = 0; i < editor->line_count; i++) {
        char* line = editor->buffer[i];
        int len = strlen(line);
        
        for (int j = 0; j < len; j++) {
            content[pos++] = line[j];
        }
        
        if (i < editor->line_count - 1) {
            content[pos++] = '\n';
        }
    }
    content[pos] = '\0';
    
    // Save to file
    fs_node_t* node = fs_resolve_path(editor->filename);
    if (!node) {
        // Create new file
        fs_node_t* parent = fs_get_current_dir();
        node = fs_create_file(editor->filename, FILE_TYPE_REGULAR);
        if (node) {
            fs_add_child(parent, node);
        }
    }
    
    if (node && fs_write_file(node, content, pos) == 0) {
        editor->modified = 0;
        editor_set_status(editor, "File saved");
    } else {
        editor_set_status(editor, "Error saving file");
    }
}

// Process commands (like :w, :q, :wq)
void editor_process_command(editor_state_t* editor) {
    if (strcmp(editor->command_buffer, "w") == 0) {
        editor_save_file(editor);
    } else if (strcmp(editor->command_buffer, "q") == 0) {
        if (editor->modified) {
            editor_set_status(editor, "No write since last change (add ! to override)");
        } else {
            // Exit editor
            editor->mode = -1; // Signal to exit
        }
    } else if (strcmp(editor->command_buffer, "q!") == 0) {
        // Force quit
        editor->mode = -1;
    } else if (strcmp(editor->command_buffer, "wq") == 0) {
        editor_save_file(editor);
        if (!editor->modified) {
            editor->mode = -1;
        }
    } else {
        editor_set_status(editor, "Unknown command");
    }
    
    // Clear command buffer
    editor->command_length = 0;
    editor->command_buffer[0] = '\0';
}

// Process keyboard input
void editor_process_key(editor_state_t* editor, char key, uint8_t scancode) {
    if (editor->mode == MODE_NORMAL) {
        // Normal mode commands
        switch (key) {
            case 'i':
                editor->mode = MODE_INSERT;
                editor_set_status(editor, "-- INSERT --");
                break;
            case 'a':
                editor->cursor_x++;
                editor_move_cursor(editor, 0, 0); // Bounds check
                editor->mode = MODE_INSERT;
                editor_set_status(editor, "-- INSERT --");
                break;
            case 'o':
                // Insert line below
                if (editor->line_count < EDITOR_MAX_LINES - 1) {
                    editor->cursor_x = 0;
                    editor->cursor_y++;
                    
                    // Shift lines down
                    for (int i = editor->line_count; i >= editor->cursor_y; i--) {
                        strcpy(editor->buffer[i], editor->buffer[i - 1]);
                    }
                    editor->buffer[editor->cursor_y][0] = '\0';
                    editor->line_count++;
                    
                    editor->mode = MODE_INSERT;
                    editor->modified = 1;
                    editor_set_status(editor, "-- INSERT --");
                }
                break;
            case 'h':
                editor_move_cursor(editor, -1, 0);
                break;
            case 'j':
                editor_move_cursor(editor, 0, 1);
                break;
            case 'k':
                editor_move_cursor(editor, 0, -1);
                break;
            case 'l':
                editor_move_cursor(editor, 1, 0);
                break;
            case ':':
                editor->mode = MODE_COMMAND;
                editor->command_length = 0;
                editor->command_buffer[0] = '\0';
                break;
            case 'x':
                // Delete character under cursor
                if (editor->cursor_x < strlen(editor->buffer[editor->cursor_y])) {
                    char* line = editor->buffer[editor->cursor_y];
                    int len = strlen(line);
                    for (int i = editor->cursor_x; i < len; i++) {
                        line[i] = line[i + 1];
                    }
                    editor->modified = 1;
                }
                break;
            case 'd':
                // dd - delete line (simplified)
                if (editor->line_count > 1) {
                    // Shift lines up
                    for (int i = editor->cursor_y; i < editor->line_count - 1; i++) {
                        strcpy(editor->buffer[i], editor->buffer[i + 1]);
                    }
                    editor->line_count--;
                    if (editor->cursor_y >= editor->line_count) {
                        editor->cursor_y = editor->line_count - 1;
                    }
                    editor->cursor_x = 0;
                    editor->modified = 1;
                }
                break;
        }
        
        // Handle arrow keys in normal mode
        if (scancode == SCANCODE_UP) editor_move_cursor(editor, 0, -1);
        else if (scancode == SCANCODE_DOWN) editor_move_cursor(editor, 0, 1);
        else if (scancode == SCANCODE_LEFT) editor_move_cursor(editor, -1, 0);
        else if (scancode == SCANCODE_RIGHT) editor_move_cursor(editor, 1, 0);
        
    } else if (editor->mode == MODE_INSERT) {
        // Insert mode
        if (scancode == SCANCODE_ESC) {
            editor->mode = MODE_NORMAL;
            editor_set_status(editor, "-- NORMAL --");
            if (editor->cursor_x > 0) editor->cursor_x--;
        } else if (scancode == SCANCODE_BACKSPACE) {
            editor_delete_char(editor);
        } else if (scancode == SCANCODE_ENTER) {
            editor_insert_char(editor, '\n');
        } else if (scancode == SCANCODE_UP) {
            editor_move_cursor(editor, 0, -1);
        } else if (scancode == SCANCODE_DOWN) {
            editor_move_cursor(editor, 0, 1);
        } else if (scancode == SCANCODE_LEFT) {
            editor_move_cursor(editor, -1, 0);
        } else if (scancode == SCANCODE_RIGHT) {
            editor_move_cursor(editor, 1, 0);
        } else if (key != 0) {
            editor_insert_char(editor, key);
        }
        
    } else if (editor->mode == MODE_COMMAND) {
        // Command mode
        if (scancode == SCANCODE_ESC) {
            editor->mode = MODE_NORMAL;
            editor->command_length = 0;
            editor_set_status(editor, "-- NORMAL --");
        } else if (scancode == SCANCODE_ENTER) {
            editor_process_command(editor);
            if (editor->mode != -1) {
                editor->mode = MODE_NORMAL;
            }
        } else if (scancode == SCANCODE_BACKSPACE) {
            if (editor->command_length > 0) {
                editor->command_length--;
                editor->command_buffer[editor->command_length] = '\0';
            }
        } else if (key != 0 && editor->command_length < 78) {
            editor->command_buffer[editor->command_length++] = key;
            editor->command_buffer[editor->command_length] = '\0';
        }
    }
} 