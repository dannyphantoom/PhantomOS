#include "filesystem.h"

// Simple memory allocator for file system
static char memory_pool[64 * 1024]; // 64KB memory pool
static size_t memory_offset = 0;
static uint32_t time_counter = 0;

// Global file system instance
static filesystem_t fs;

// Forward declarations
char* strcat(char* dest, const char* src);

// Simple memory allocation
void* kmalloc(size_t size) {
    if (memory_offset + size >= sizeof(memory_pool)) {
        return NULL; // Out of memory
    }
    void* ptr = &memory_pool[memory_offset];
    memory_offset += size;
    return ptr;
}

void kfree(void* ptr) {
    // Simple allocator - no deallocation for now
    (void)ptr;
}

// Memory utility functions
void* memset(void* ptr, int value, size_t size) {
    unsigned char* p = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t size) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}

// String utility functions
void strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

// Simple strcat implementation
char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);
    while (*src) {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

uint32_t get_current_time(void) {
    return ++time_counter;
}

// Initialize the file system
void fs_init(void) {
    memset(&fs, 0, sizeof(filesystem_t));
    
    // Create root directory
    fs.root = fs_create_file("/", FILE_TYPE_DIRECTORY);
    if (!fs.root) {
        terminal_writestring("Error: Failed to create root directory\n");
        return;
    }
    
    fs.current_dir = fs.root;
    fs.root->parent = fs.root; // Root is its own parent
    strcpy(fs.current_path, "/");
    
    terminal_writestring("File system initialized\n");
}

// Create a new file or directory
fs_node_t* fs_create_file(const char* name, file_type_t type) {
    if (fs.total_files >= MAX_TOTAL_FILES) {
        return NULL; // Too many files
    }
    
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) {
        return NULL;
    }
    
    memset(node, 0, sizeof(fs_node_t));
    strncpy(node->name, name, MAX_FILENAME_LENGTH - 1);
    node->type = type;
    node->creation_time = get_current_time();
    node->modification_time = node->creation_time;
    
    if (type == FILE_TYPE_DIRECTORY) {
        node->data = NULL; // Children array is embedded in struct
        node->size = 0;
    } else {
        // Allocate data buffer for regular files
        node->data = kmalloc(MAX_FILE_SIZE);
        if (node->data) {
            memset(node->data, 0, MAX_FILE_SIZE);
        }
        node->size = 0;
    }
    
    // Add to global file list
    node->next = fs.file_list_head;
    fs.file_list_head = node;
    fs.total_files++;
    
    return node;
}

// Find a child by name in a directory
fs_node_t* fs_find_child(fs_node_t* parent, const char* name) {
    if (!parent || parent->type != FILE_TYPE_DIRECTORY) {
        return NULL;
    }
    
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] && strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    
    return NULL;
}

// Add a child to a directory
int fs_add_child(fs_node_t* parent, fs_node_t* child) {
    if (!parent || !child || parent->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    
    if (parent->child_count >= MAX_FILES_PER_DIR) {
        return -1; // Directory full
    }
    
    // Check if name already exists
    if (fs_find_child(parent, child->name)) {
        return -1; // Name collision
    }
    
    parent->children[parent->child_count] = child;
    parent->child_count++;
    child->parent = parent;
    parent->modification_time = get_current_time();
    
    return 0;
}

// Remove a child from a directory
int fs_remove_child(fs_node_t* parent, const char* name) {
    if (!parent || parent->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] && strcmp(parent->children[i]->name, name) == 0) {
            // Shift remaining children
            for (size_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->children[parent->child_count - 1] = NULL;
            parent->child_count--;
            parent->modification_time = get_current_time();
            return 0;
        }
    }
    
    return -1; // Not found
}

// Resolve a path to a file system node
fs_node_t* fs_resolve_path(const char* path) {
    if (!path || strlen(path) == 0) {
        return fs.current_dir;
    }
    
    fs_node_t* current;
    
    // Absolute or relative path?
    if (path[0] == '/') {
        current = fs.root;
        path++; // Skip leading slash
    } else {
        current = fs.current_dir;
    }
    
    // Handle root directory
    if (strlen(path) == 0) {
        return current;
    }
    
    // Parse path components
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, path, MAX_PATH_LENGTH - 1);
    
    char* token = path_copy;
    char* next_token;
    
    while (token && *token) {
        // Find next path separator
        next_token = token;
        while (*next_token && *next_token != '/') {
            next_token++;
        }
        
        if (*next_token == '/') {
            *next_token = '\0';
            next_token++;
        } else {
            next_token = NULL;
        }
        
        // Handle special cases
        if (strcmp(token, ".") == 0) {
            // Current directory - do nothing
        } else if (strcmp(token, "..") == 0) {
            // Parent directory
            current = current->parent;
        } else {
            // Regular directory/file name
            current = fs_find_child(current, token);
            if (!current) {
                return NULL; // Path not found
            }
        }
        
        token = next_token;
    }
    
    return current;
}

// Change current directory
int fs_change_directory(const char* path) {
    fs_node_t* target = fs_resolve_path(path);
    
    if (!target) {
        return -1; // Path not found
    }
    
    if (target->type != FILE_TYPE_DIRECTORY) {
        return -2; // Not a directory
    }
    
    fs.current_dir = target;
    fs_update_current_path();
    return 0;
}

// Update current path string
void fs_update_current_path(void) {
    if (fs.current_dir == fs.root) {
        strcpy(fs.current_path, "/");
        return;
    }
    
    // Build path by traversing up to root
    char temp_path[MAX_PATH_LENGTH];
    char components[16][MAX_FILENAME_LENGTH]; // Max 16 levels deep
    int depth = 0;
    
    fs_node_t* node = fs.current_dir;
    while (node != fs.root && depth < 16) {
        strcpy(components[depth], node->name);
        depth++;
        node = node->parent;
    }
    
    // Build path string
    strcpy(fs.current_path, "/");
    for (int i = depth - 1; i >= 0; i--) {
        if (strlen(fs.current_path) > 1) {
            strcat(fs.current_path, "/");
        }
        strcat(fs.current_path, components[i]);
    }
}

// Get current directory
fs_node_t* fs_get_current_dir(void) {
    return fs.current_dir;
}

// Get current path
char* fs_get_current_path(void) {
    return fs.current_path;
}

// Write data to a file
int fs_write_file(fs_node_t* file, const char* data, size_t size) {
    if (!file || file->type != FILE_TYPE_REGULAR || !file->data) {
        return -1;
    }
    
    if (size > MAX_FILE_SIZE) {
        size = MAX_FILE_SIZE;
    }
    
    memcpy(file->data, data, size);
    file->size = size;
    file->modification_time = get_current_time();
    
    return (int)size;
}

// Read data from a file
char* fs_read_file(fs_node_t* file) {
    if (!file || file->type != FILE_TYPE_REGULAR || !file->data) {
        return NULL;
    }
    
    return (char*)file->data;
}

// Delete a file system node
int fs_delete_node(fs_node_t* node) {
    if (!node) {
        return -1;
    }
    
    // Can't delete root
    if (node == fs.root) {
        return -1;
    }
    
    // For directories, must be empty
    if (node->type == FILE_TYPE_DIRECTORY && node->child_count > 0) {
        return -1;
    }
    
    // Remove from parent
    if (node->parent) {
        fs_remove_child(node->parent, node->name);
    }
    
    // Remove from global list (simplified - just mark as deleted)
    // In a real implementation, we'd properly manage the linked list
    
    fs.total_files--;
    
    return 0;
}

// Copy file
int fs_copy_file(const char* src_path, const char* dest_path) {
    fs_node_t* src = fs_resolve_path(src_path);
    if (!src || src->type != FILE_TYPE_REGULAR) {
        return -1;
    }
    
    // Extract destination directory and filename
    char dest_dir_path[MAX_PATH_LENGTH];
    char dest_filename[MAX_FILENAME_LENGTH];
    
    fs_get_parent_path(dest_path, dest_dir_path);
    fs_get_filename(dest_path, dest_filename);
    
    fs_node_t* dest_dir = fs_resolve_path(dest_dir_path);
    if (!dest_dir || dest_dir->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    
    // Create new file
    fs_node_t* dest_file = fs_create_file(dest_filename, FILE_TYPE_REGULAR);
    if (!dest_file) {
        return -1;
    }
    
    // Copy data
    if (src->data && src->size > 0) {
        fs_write_file(dest_file, (char*)src->data, src->size);
    }
    
    // Add to destination directory
    if (fs_add_child(dest_dir, dest_file) != 0) {
        fs_delete_node(dest_file);
        return -1;
    }
    
    return 0;
}

// Move file
int fs_move_file(const char* src_path, const char* dest_path) {
    if (fs_copy_file(src_path, dest_path) == 0) {
        fs_node_t* src = fs_resolve_path(src_path);
        if (src) {
            fs_delete_node(src);
            return 0;
        }
    }
    return -1;
}

// Utility functions for path manipulation
void fs_get_parent_path(const char* path, char* parent) {
    strcpy(parent, path);
    
    // Find last slash
    int last_slash = -1;
    for (int i = strlen(parent) - 1; i >= 0; i--) {
        if (parent[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash <= 0) {
        strcpy(parent, "/");
    } else {
        parent[last_slash] = '\0';
    }
}

void fs_get_filename(const char* path, char* filename) {
    // Find last slash
    int last_slash = -1;
    for (int i = strlen(path) - 1; i >= 0; i--) {
        if (path[i] == '/') {
            last_slash = i;
            break;
        }
    }
    
    if (last_slash == -1) {
        strcpy(filename, path);
    } else {
        strcpy(filename, &path[last_slash + 1]);
    }
} 