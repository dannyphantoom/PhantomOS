#ifndef EDITOR_H
#define EDITOR_H

#include "kernel.h"
#include "filesystem.h"

// Editor constants
#define EDITOR_MAX_LINES 50
#define EDITOR_MAX_LINE_LENGTH 76
#define EDITOR_TAB_SIZE 4

// Editor modes
typedef enum {
    MODE_NORMAL = 0,
    MODE_INSERT = 1,
    MODE_COMMAND = 2
} editor_mode_t;

// Editor state structure
typedef struct {
    char buffer[EDITOR_MAX_LINES][EDITOR_MAX_LINE_LENGTH];
    int line_count;
    int cursor_x;
    int cursor_y;
    int view_start_line;  // For scrolling
    editor_mode_t mode;
    char filename[MAX_FILENAME_LENGTH];
    int modified;
    char command_buffer[80];
    int command_length;
    char status_message[80];
} editor_state_t;

// Function declarations
void editor_init(editor_state_t* editor);
void editor_open(editor_state_t* editor, const char* filename);
void editor_draw(editor_state_t* editor);
void editor_process_key(editor_state_t* editor, char key, uint8_t scancode);
void editor_insert_char(editor_state_t* editor, char c);
void editor_delete_char(editor_state_t* editor);
void editor_save_file(editor_state_t* editor);
void editor_set_status(editor_state_t* editor, const char* message);
void editor_move_cursor(editor_state_t* editor, int dx, int dy);
void editor_process_command(editor_state_t* editor);

#endif // EDITOR_H 