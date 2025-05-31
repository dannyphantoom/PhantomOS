#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "kernel.h"

// File system constants
#define MAX_FILENAME_LENGTH 64
#define MAX_PATH_LENGTH 256
#define MAX_FILES_PER_DIR 32
#define MAX_FILE_SIZE 4096
#define MAX_TOTAL_FILES 128

// File types
typedef enum {
    FILE_TYPE_REGULAR = 0,
    FILE_TYPE_DIRECTORY = 1
} file_type_t;

// File system node structure
typedef struct fs_node {
    char name[MAX_FILENAME_LENGTH];
    file_type_t type;
    size_t size;
    uint32_t creation_time;
    uint32_t modification_time;
    
    // For files: pointer to data
    // For directories: pointer to children array
    void* data;
    
    // Directory management
    struct fs_node* parent;
    struct fs_node* children[MAX_FILES_PER_DIR];
    size_t child_count;
    
    // Linked list for global file tracking
    struct fs_node* next;
} fs_node_t;

// File system state
typedef struct {
    fs_node_t* root;
    fs_node_t* current_dir;
    fs_node_t* file_list_head;
    size_t total_files;
    char current_path[MAX_PATH_LENGTH];
} filesystem_t;

// Function declarations
void fs_init(void);
fs_node_t* fs_create_file(const char* name, file_type_t type);
fs_node_t* fs_find_child(fs_node_t* parent, const char* name);
fs_node_t* fs_resolve_path(const char* path);
int fs_add_child(fs_node_t* parent, fs_node_t* child);
int fs_remove_child(fs_node_t* parent, const char* name);
int fs_delete_node(fs_node_t* node);
void fs_update_current_path(void);
char* fs_get_current_path(void);
fs_node_t* fs_get_current_dir(void);
int fs_change_directory(const char* path);

// File operations
int fs_write_file(fs_node_t* file, const char* data, size_t size);
char* fs_read_file(fs_node_t* file);
int fs_copy_file(const char* src_path, const char* dest_path);
int fs_move_file(const char* src_path, const char* dest_path);

// Path utilities
void fs_normalize_path(const char* input, char* output);
void fs_get_parent_path(const char* path, char* parent);
void fs_get_filename(const char* path, char* filename);

#endif // FILESYSTEM_H 