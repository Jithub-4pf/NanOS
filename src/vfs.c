/*
 * vfs.c - Virtual Filesystem Layer for NanOS
 *
 * Provides a unified API for file and directory operations, path resolution,
 * and integration with the ext2 filesystem. All public APIs are documented
 * in include/vfs.h. Error codes: 0 = success, -1 = error.
 */
#include "vfs.h"
#include "ext2.h"
#include "blockdev.h"
#include "ramdisk.h"
#include "heap.h"
#include "monitor.h"

// Maximum symlink resolution depth to prevent loops
#define MAX_SYMLINK_DEPTH 8

// Simple string functions
static void strcpy_local(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static int strcmp_local(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

static size_t strlen_local(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// Helper to get file type from inode mode
static int vfs_get_type_from_mode(uint16_t mode) {
    if ((mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) return VFS_TYPE_DIR;
    if ((mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) return VFS_TYPE_SYMLINK;
    return VFS_TYPE_FILE;
}

// Resolve symlink to its target path
static int vfs_resolve_symlink(ext2_fs_t* fs, uint32_t inode_num, char* resolved_path, size_t max_len, int depth) {
    if (depth > MAX_SYMLINK_DEPTH) return -1; // Too many levels of symlinks
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) return -1;
    
    // Check if it's actually a symlink
    if ((inode.i_mode & EXT2_S_IFLNK) != EXT2_S_IFLNK) {
        return -1; // Not a symlink
    }
    
    char target[256];
    if (ext2_read_symlink(fs, &inode, target, sizeof(target)) != 0) return -1;
    
    // If target is absolute path, use it directly
    if (target[0] == '/') {
        if (strlen_local(target) >= max_len) return -1;
        strcpy_local(resolved_path, target);
    } else {
        // Relative path - would need to resolve relative to symlink's directory
        // For now, treat as absolute (simplified)
        if (strlen_local(target) >= max_len) return -1;
        strcpy_local(resolved_path, target);
    }
    
    return 0;
}

// VFS initialization
/**
 * Initialize the virtual filesystem layer.
 * @return 0 on success, -1 on error
 */
int vfs_init(void) {
    monitor_write("[VFS] Initializing virtual filesystem\n");
    return 0;
}

/**
 * Mount the root filesystem (usually ext2 on ramdisk0).
 * @return 0 on success, -1 on error
 */
int vfs_mount_root(void) {
    // Get the root device (assuming it's "ramdisk0")
    blockdev_t* root_device = blockdev_get("ramdisk0");
    if (!root_device) {
        monitor_write("[VFS] Root device not found\n");
        return -1;
    }
    
    // Mount the ext2 filesystem
    if (ext2_mount(root_device) != 0) {
        monitor_write("[VFS] Failed to mount root filesystem\n");
        return -1;
    }
    
    monitor_write("[VFS] Root filesystem mounted successfully\n");
    return 0;
}

// Parse path and resolve to inode
static int vfs_resolve_path(const char* path, uint32_t* out_inode) {
    if (!path || !out_inode) return -1;
    
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs) return -1;
    
    // Start from root inode
    uint32_t current_inode = EXT2_ROOT_INO;
    
    // Handle root directory
    if (strcmp_local(path, "/") == 0) {
        *out_inode = EXT2_ROOT_INO;
        return 0;
    }
    
    // Skip leading slash
    if (path[0] == '/') path++;
    
    // Parse path components
    char component[256];
    int comp_idx = 0;
    
    while (*path) {
        if (*path == '/') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';
                
                // Read current directory inode
                ext2_inode_t dir_inode;
                if (ext2_read_inode(fs, current_inode, &dir_inode) != 0) {
                    return -1;
                }
                
                // Find the component in the directory
                if (ext2_find_dir_entry(fs, &dir_inode, component, &current_inode) != 0) {
                    return -1;
                }
                
                comp_idx = 0;
            }
            path++;
        } else {
            if (comp_idx < 255) {
                component[comp_idx++] = *path;
            }
            path++;
        }
    }
    
    // Handle final component
    if (comp_idx > 0) {
        component[comp_idx] = '\0';
        
        ext2_inode_t dir_inode;
        if (ext2_read_inode(fs, current_inode, &dir_inode) != 0) {
            return -1;
        }
        
        if (ext2_find_dir_entry(fs, &dir_inode, component, &current_inode) != 0) {
            return -1;
        }
    }
    
    *out_inode = current_inode;
    return 0;
}

// Open file
vfs_file_t* vfs_open(const char* path) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs) return NULL;
    
    uint32_t inode_num = ext2_path_to_inode(fs, path);
    if (inode_num == 0) return NULL;
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) return NULL;
    
    // Handle symlinks - follow them to get the actual file
    int depth = 0;
    while ((inode.i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK && depth < MAX_SYMLINK_DEPTH) {
        char target[256];
        if (ext2_read_symlink(fs, &inode, target, sizeof(target)) != 0) return NULL;
        
        // Resolve the target path
        inode_num = ext2_path_to_inode(fs, target);
        if (inode_num == 0) return NULL;
        
        if (ext2_read_inode(fs, inode_num, &inode) != 0) return NULL;
        depth++;
    }
    
    if (depth >= MAX_SYMLINK_DEPTH) return NULL; // Too many symlinks
    
    // Don't allow opening directories
    if ((inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) return NULL;
    
    vfs_file_t* file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    if (!file) return NULL;
    
    file->inode = inode_num;
    file->size = inode.i_size;
    file->type = VFS_TYPE_FILE;
    file->position = 0;
    file->is_open = 1;
    
    return file;
}

// Close file
int vfs_close(vfs_file_t* file) {
    if (!file || !file->is_open) return -1;
    
    file->is_open = 0;
    kfree(file);
    return 0;
}

// Read from file
int vfs_read(vfs_file_t* file, void* buffer, uint32_t size) {
    if (!file || !file->is_open || !buffer) return -1;
    
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, file->inode, &inode) != 0) {
        return -1;
    }
    
    int bytes_read = ext2_read_file(fs, &inode, file->position, size, buffer);
    if (bytes_read > 0) {
        file->position += bytes_read;
        
        // Update access time
        extern uint32_t system_ticks;
        inode.i_atime = system_ticks / 100;
        ext2_write_inode(fs, file->inode, &inode);
    }
    
    return bytes_read;
}

// Seek in file
int vfs_seek(vfs_file_t* file, uint32_t offset) {
    if (!file || !file->is_open) return -1;
    
    if (offset > file->size) {
        file->position = file->size;
    } else {
        file->position = offset;
    }
    
    return 0;
}

// Get file/directory stats
int vfs_stat(const char* path, vfs_dirent_t* stat) {
    if (!path || !stat) return -1;
    
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs) return -1;
    
    uint32_t inode_num = ext2_path_to_inode(fs, path);
    if (inode_num == 0) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) return -1;
    
    // Get the last component of the path for the name
    const char* name = path;
    for (const char* p = path; *p; p++) {
        if (*p == '/' && *(p + 1)) name = p + 1;
    }
    
    stat->inode = inode_num;
    strcpy_local(stat->name, name);
    stat->type = vfs_get_type_from_mode(inode.i_mode);
    stat->size = inode.i_size;
    
    return 0;
}

// Check if path exists
int vfs_exists(const char* path) {
    uint32_t inode_num;
    return vfs_resolve_path(path, &inode_num) == 0 ? 1 : 0;
}

// Simple directory listing (for shell ls command)
int vfs_list_directory(const char* path) {
    if (!path) return -1;
    
    uint32_t inode_num;
    if (vfs_resolve_path(path, &inode_num) != 0) {
        return -1;
    }
    
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) {
        return -1;
    }
    
    if ((inode.i_mode & EXT2_S_IFDIR) == 0) {
        monitor_write("Not a directory\n");
        return -1;
    }
    
    return ext2_list_dir(fs, &inode);
} 

// --- Writable VFS API implementation ---

// Create a file or directory
int vfs_create(const char* path, int type) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !path) return -1;
    // Split path into parent and name
    char parent_path[256], name[256];
    int len = strlen_local(path);
    if (len == 0 || len > 255) return -1;
    int last_slash = -1;
    for (int i = 0; i < len; ++i) if (path[i] == '/') last_slash = i;
    if (last_slash < 0) return -1;
    if (last_slash == 0) {
        parent_path[0] = '/'; parent_path[1] = 0;
    } else {
        memcpy_local(parent_path, path, last_slash);
        parent_path[last_slash] = 0;
    }
    strcpy_local(name, path + last_slash + 1);
    if (strlen_local(name) == 0) return -1;
    // Find parent inode
    uint32_t parent_inode_num;
    if (vfs_resolve_path(parent_path, &parent_inode_num) != 0) return -1;
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, parent_inode_num, &parent_inode) != 0) return -1;
    // Allocate inode
    uint32_t new_inode_num = ext2_alloc_inode(fs);
    if (new_inode_num == 0) return -1;
    ext2_inode_t new_inode;
    memset_local(&new_inode, 0, sizeof(ext2_inode_t));
    if (type == VFS_TYPE_FILE) {
        new_inode.i_mode = EXT2_S_IFREG | 0644;
        new_inode.i_links_count = 1;
    } else if (type == VFS_TYPE_DIR) {
        new_inode.i_mode = EXT2_S_IFDIR | 0755;
        new_inode.i_links_count = 2; // . and ..
    } else {
        return -1;
    }
    new_inode.i_size = 0;
    new_inode.i_blocks = 0;
    
    // Set timestamps
    extern uint32_t system_ticks;
    uint32_t current_time = system_ticks / 100; // Convert to seconds
    new_inode.i_atime = current_time;
    new_inode.i_ctime = current_time;
    new_inode.i_mtime = current_time;
    
    // Write inode
    if (ext2_write_inode(fs, new_inode_num, &new_inode) != 0) return -1;
    // Add directory entry to parent
    uint8_t ext2_type = (type == VFS_TYPE_FILE) ? EXT2_FT_REG_FILE : EXT2_FT_DIR;
    if (ext2_add_dir_entry(fs, &parent_inode, parent_inode_num, new_inode_num, name, ext2_type) != 0) return -1;
    
    // Update parent directory link count if creating a directory
    if (type == VFS_TYPE_DIR) {
        parent_inode.i_links_count++;
    }
    
    // Write back parent inode
    if (ext2_write_inode(fs, parent_inode_num, &parent_inode) != 0) return -1;
    // For directories, add . and .. entries
    if (type == VFS_TYPE_DIR) {
        ext2_inode_t dir_inode;
        if (ext2_read_inode(fs, new_inode_num, &dir_inode) != 0) return -1;
        ext2_add_dir_entry(fs, &dir_inode, new_inode_num, new_inode_num, ".", EXT2_FT_DIR);
        ext2_add_dir_entry(fs, &dir_inode, new_inode_num, parent_inode_num, "..", EXT2_FT_DIR);
        ext2_write_inode(fs, new_inode_num, &dir_inode);
    }
    return 0;
}

// Remove a file or directory
int vfs_unlink(const char* path) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !path) return -1;
    // Split path into parent and name
    char parent_path[256], name[256];
    int len = strlen_local(path);
    if (len == 0 || len > 255) return -1;
    int last_slash = -1;
    for (int i = 0; i < len; ++i) if (path[i] == '/') last_slash = i;
    if (last_slash < 0) return -1;
    if (last_slash == 0) {
        parent_path[0] = '/'; parent_path[1] = 0;
    } else {
        memcpy_local(parent_path, path, last_slash);
        parent_path[last_slash] = 0;
    }
    strcpy_local(name, path + last_slash + 1);
    if (strlen_local(name) == 0) return -1;
    // Find parent inode
    uint32_t parent_inode_num;
    if (vfs_resolve_path(parent_path, &parent_inode_num) != 0) return -1;
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, parent_inode_num, &parent_inode) != 0) return -1;
    // Find inode to remove
    uint32_t inode_num;
    if (ext2_find_dir_entry(fs, &parent_inode, name, &inode_num) != 0) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) return -1;
    
    // Check if it's a directory
    if ((inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
        // Check if directory is empty
        int is_empty = ext2_is_dir_empty(fs, &inode);
        if (is_empty < 0) return -1;
        if (is_empty == 0) return -1; // Directory not empty
        
        // Update parent directory link count
        parent_inode.i_links_count--;
    }
    
    // Remove directory entry
    if (ext2_remove_dir_entry(fs, &parent_inode, parent_inode_num, name) != 0) return -1;
    // Write back parent inode
    if (ext2_write_inode(fs, parent_inode_num, &parent_inode) != 0) return -1;
    
    // Decrement link count
    inode.i_links_count--;
    
    // If link count is zero, free the inode and blocks
    if (inode.i_links_count == 0) {
        // Free blocks (simple: only direct blocks)
        for (int i = 0; i < 12; ++i) {
            if (inode.i_block[i]) ext2_free_block(fs, inode.i_block[i]);
        }
        ext2_free_inode(fs, inode_num);
    } else {
        // Write back the inode with updated link count
        ext2_write_inode(fs, inode_num, &inode);
    }
    
    return 0;
}

// Write to a file (append or overwrite at current position)
int vfs_write(vfs_file_t* file, const void* buffer, uint32_t size) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !file || !file->is_open || !buffer) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, file->inode, &inode) != 0) return -1;
    uint32_t written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;
    while (written < size) {
        uint32_t block_index = (file->position + written) / fs->block_size;
        uint32_t block_offset = (file->position + written) % fs->block_size;
        uint32_t to_write = fs->block_size - block_offset;
        if (to_write > size - written) to_write = size - written;
        // Allocate block if needed
        if (block_index < 12 && inode.i_block[block_index] == 0) {
            uint32_t new_block = ext2_alloc_block(fs);
            if (new_block == 0) break;
            inode.i_block[block_index] = new_block;
        }
        if (block_index >= 12 || inode.i_block[block_index] == 0) break;
        uint8_t* block = (uint8_t*)kmalloc(fs->block_size);
        if (!block) break;
        // Read existing block
        ext2_read_blocks(fs, inode.i_block[block_index], 1, block);
        memcpy_local(block + block_offset, buf + written, to_write);
        ext2_write_blocks(fs, inode.i_block[block_index], 1, block);
        kfree(block);
        written += to_write;
    }
    if (file->position + written > inode.i_size) inode.i_size = file->position + written;
    file->position += written;
    file->size = inode.i_size;
    
    // Update modification time
    extern uint32_t system_ticks;
    inode.i_mtime = system_ticks / 100;
    
    ext2_write_inode(fs, file->inode, &inode);
    return written;
}

// Truncate a file to new_size (only supports shrinking for now)
int vfs_truncate(vfs_file_t* file, uint32_t new_size) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !file || !file->is_open) return -1;
    ext2_inode_t inode;
    if (ext2_read_inode(fs, file->inode, &inode) != 0) return -1;
    if (new_size >= inode.i_size) return 0;
    uint32_t old_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size;
    uint32_t new_blocks = (new_size + fs->block_size - 1) / fs->block_size;
    for (uint32_t i = new_blocks; i < old_blocks && i < 12; ++i) {
        if (inode.i_block[i]) {
            ext2_free_block(fs, inode.i_block[i]);
            inode.i_block[i] = 0;
        }
    }
    inode.i_size = new_size;
    file->size = new_size;
    if (file->position > new_size) file->position = new_size;
    ext2_write_inode(fs, file->inode, &inode);
    return 0;
}

// Create a symbolic link
int vfs_create_symlink(const char* path, const char* target) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !path || !target) return -1;
    
    // Split path into parent and name
    char parent_path[256], name[256];
    int len = strlen_local(path);
    if (len == 0 || len > 255) return -1;
    int last_slash = -1;
    for (int i = 0; i < len; ++i) if (path[i] == '/') last_slash = i;
    if (last_slash < 0) return -1;
    if (last_slash == 0) {
        parent_path[0] = '/'; parent_path[1] = 0;
    } else {
        memcpy_local(parent_path, path, last_slash);
        parent_path[last_slash] = 0;
    }
    strcpy_local(name, path + last_slash + 1);
    if (strlen_local(name) == 0) return -1;
    
    // Find parent inode
    uint32_t parent_inode_num;
    if (vfs_resolve_path(parent_path, &parent_inode_num) != 0) return -1;
    ext2_inode_t parent_inode;
    if (ext2_read_inode(fs, parent_inode_num, &parent_inode) != 0) return -1;
    
    // Allocate inode for symlink
    uint32_t new_inode_num = ext2_alloc_inode(fs);
    if (new_inode_num == 0) return -1;
    
    ext2_inode_t new_inode;
    memset_local(&new_inode, 0, sizeof(ext2_inode_t));
    new_inode.i_mode = EXT2_S_IFLNK | 0777; // Symlinks usually have all permissions
    new_inode.i_links_count = 1;
    
    uint32_t target_len = strlen_local(target);
    new_inode.i_size = target_len;
    
    // Store symlink target
    if (target_len <= 60) {
        // Fast symlink: store in i_block array
        memcpy_local((char*)new_inode.i_block, target, target_len);
    } else {
        // Slow symlink: store in data block
        uint32_t block = ext2_alloc_block(fs);
        if (block == 0) {
            ext2_free_inode(fs, new_inode_num);
            return -1;
        }
        new_inode.i_block[0] = block;
        char* block_data = (char*)kmalloc(fs->block_size);
        memset_local(block_data, 0, fs->block_size);
        memcpy_local(block_data, target, target_len);
        ext2_write_blocks(fs, block, 1, block_data);
        kfree(block_data);
    }
    
    // Write inode
    if (ext2_write_inode(fs, new_inode_num, &new_inode) != 0) {
        ext2_free_inode(fs, new_inode_num);
        return -1;
    }
    
    // Add directory entry to parent
    if (ext2_add_dir_entry(fs, &parent_inode, parent_inode_num, new_inode_num, name, EXT2_FT_SYMLINK) != 0) {
        ext2_free_inode(fs, new_inode_num);
        return -1;
    }
    
    // Write back parent inode
    if (ext2_write_inode(fs, parent_inode_num, &parent_inode) != 0) return -1;
    
    return 0;
} 

// Change file permissions
int vfs_chmod(const char* path, uint16_t mode) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !path) return -1;
    
    uint32_t inode_num = ext2_path_to_inode(fs, path);
    if (inode_num == 0) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) return -1;
    
    // Update mode bits (preserve file type bits)
    uint16_t file_type = inode.i_mode & 0xF000;
    inode.i_mode = file_type | (mode & 0777);
    
    // Update change time
    extern uint32_t system_ticks;
    inode.i_ctime = system_ticks / 100;
    
    // Write back inode
    if (ext2_write_inode(fs, inode_num, &inode) != 0) return -1;
    
    return 0;
}

// Change file ownership
int vfs_chown(const char* path, uint16_t uid, uint16_t gid) {
    ext2_fs_t* fs = ext2_get_fs();
    if (!fs || !path) return -1;
    
    uint32_t inode_num = ext2_path_to_inode(fs, path);
    if (inode_num == 0) return -1;
    
    ext2_inode_t inode;
    if (ext2_read_inode(fs, inode_num, &inode) != 0) return -1;
    
    // Update ownership
    inode.i_uid = uid;
    inode.i_gid = gid;
    
    // Update change time
    extern uint32_t system_ticks;
    inode.i_ctime = system_ticks / 100;
    
    // Write back inode
    if (ext2_write_inode(fs, inode_num, &inode) != 0) return -1;
    
    return 0;
} 