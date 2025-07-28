#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

// VFS file types
#define VFS_TYPE_FILE 1
#define VFS_TYPE_DIR  2
#define VFS_TYPE_SYMLINK 3

// VFS file descriptor
typedef struct vfs_file {
    uint32_t inode;
    uint32_t size;
    uint32_t type;
    uint32_t position;
    int is_open;
} vfs_file_t;

// VFS directory entry
typedef struct vfs_dirent {
    uint32_t inode;
    char name[256];
    uint32_t type;
    uint32_t size;
} vfs_dirent_t;

/**
 * Initialize the virtual filesystem layer.
 * @return 0 on success, -1 on error
 */
int vfs_init(void);

/**
 * Mount the root filesystem (usually ext2 on ramdisk0).
 * @return 0 on success, -1 on error
 */
int vfs_mount_root(void);

/**
 * Open a file for reading or writing.
 * @param path Absolute path to file
 * @return Pointer to vfs_file_t on success, NULL on error
 */
vfs_file_t* vfs_open(const char* path);

/**
 * Close an open file.
 * @param file File handle
 * @return 0 on success, -1 on error
 */
int vfs_close(vfs_file_t* file);

/**
 * Read from an open file.
 * @param file File handle
 * @param buffer Buffer to read into
 * @param size Number of bytes to read
 * @return Number of bytes read, or -1 on error
 */
int vfs_read(vfs_file_t* file, void* buffer, uint32_t size);

/**
 * Seek to a position in an open file.
 * @param file File handle
 * @param offset Offset in bytes
 * @return 0 on success, -1 on error
 */
int vfs_seek(vfs_file_t* file, uint32_t offset);

/**
 * Open a directory for reading.
 * @param path Absolute path to directory
 * @return Directory handle (int), or -1 on error
 */
int vfs_opendir(const char* path);

/**
 * Read a directory entry from an open directory.
 * @param dir_fd Directory handle
 * @param entry Output entry
 * @return 0 on success, -1 on error or end of directory
 */
int vfs_readdir(int dir_fd, vfs_dirent_t* entry);

/**
 * Close an open directory.
 * @param dir_fd Directory handle
 * @return 0 on success, -1 on error
 */
int vfs_closedir(int dir_fd);

/**
 * Create a file or directory.
 * @param path Absolute path
 * @param type VFS_TYPE_FILE or VFS_TYPE_DIR
 * @return 0 on success, -1 on error
 */
int vfs_create(const char* path, int type);

/**
 * Remove a file or directory.
 * @param path Absolute path
 * @return 0 on success, -1 on error
 */
int vfs_unlink(const char* path);

/**
 * Write to an open file.
 * @param file File handle
 * @param buffer Data to write
 * @param size Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
int vfs_write(vfs_file_t* file, const void* buffer, uint32_t size);

/**
 * Truncate a file to a new size.
 * @param file File handle
 * @param new_size New size in bytes
 * @return 0 on success, -1 on error
 */
int vfs_truncate(vfs_file_t* file, uint32_t new_size);

/**
 * Create a symbolic link.
 * @param path Link path
 * @param target Target path
 * @return 0 on success, -1 on error
 */
int vfs_create_symlink(const char* path, const char* target);

/**
 * Change file permissions.
 * @param path File path
 * @param mode New mode (octal)
 * @return 0 on success, -1 on error
 */
int vfs_chmod(const char* path, uint16_t mode);

/**
 * Change file ownership.
 * @param path File path
 * @param uid New user ID
 * @param gid New group ID
 * @return 0 on success, -1 on error
 */
int vfs_chown(const char* path, uint16_t uid, uint16_t gid);

/**
 * Get file or directory stats.
 * @param path File or directory path
 * @param stat Output stats
 * @return 0 on success, -1 on error
 */
int vfs_stat(const char* path, vfs_dirent_t* stat);

/**
 * Check if a path exists.
 * @param path File or directory path
 * @return 1 if exists, 0 if not
 */
int vfs_exists(const char* path);

/**
 * List directory contents (for shell ls command).
 * @param path Directory path
 * @return 0 on success, -1 on error
 */
int vfs_list_directory(const char* path);

#endif // VFS_H 