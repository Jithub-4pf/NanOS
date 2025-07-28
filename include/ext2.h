#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <stddef.h>
#include "blockdev.h"

// ext2 magic number
#define EXT2_MAGIC 0xEF53

// ext2 block sizes
#define EXT2_MIN_BLOCK_SIZE 1024
#define EXT2_MAX_BLOCK_SIZE 4096

// ext2 superblock location
#define EXT2_SUPERBLOCK_OFFSET 1024

// ext2 inode types
#define EXT2_S_IFSOCK 0xC000 // Socket
#define EXT2_S_IFLNK  0xA000 // Symbolic link
#define EXT2_S_IFREG  0x8000 // Regular file
#define EXT2_S_IFBLK  0x6000 // Block device
#define EXT2_S_IFDIR  0x4000 // Directory
#define EXT2_S_IFCHR  0x2000 // Character device
#define EXT2_S_IFIFO  0x1000 // FIFO

// ext2 filesystem states
#define EXT2_VALID_FS 1   // Filesystem is clean
#define EXT2_ERROR_FS 2   // Filesystem has errors

// ext2 error handling methods
#define EXT2_ERRORS_CONTINUE 1  // Continue on error
#define EXT2_ERRORS_RO       2  // Remount read-only on error
#define EXT2_ERRORS_PANIC    3  // Panic on error

// ext2 superblock structure
typedef struct ext2_superblock {
    uint32_t s_inodes_count;        // Total number of inodes
    uint32_t s_blocks_count;        // Total number of blocks
    uint32_t s_r_blocks_count;      // Reserved blocks count
    uint32_t s_free_blocks_count;   // Free blocks count
    uint32_t s_free_inodes_count;   // Free inodes count
    uint32_t s_first_data_block;    // First data block
    uint32_t s_log_block_size;      // Block size = 1024 << s_log_block_size
    uint32_t s_log_frag_size;       // Fragment size
    uint32_t s_blocks_per_group;    // Blocks per group
    uint32_t s_frags_per_group;     // Fragments per group
    uint32_t s_inodes_per_group;    // Inodes per group
    uint32_t s_mtime;               // Mount time
    uint32_t s_wtime;               // Write time
    uint16_t s_mnt_count;           // Mount count
    uint16_t s_max_mnt_count;       // Maximum mount count
    uint16_t s_magic;               // Magic signature
    uint16_t s_state;               // Filesystem state
    uint16_t s_errors;              // Error handling method
    uint16_t s_minor_rev_level;     // Minor revision level
    uint32_t s_lastcheck;           // Last check time
    uint32_t s_checkinterval;       // Check interval
    uint32_t s_creator_os;          // Creator OS
    uint32_t s_rev_level;           // Revision level
    uint16_t s_def_resuid;          // Default uid for reserved blocks
    uint16_t s_def_resgid;          // Default gid for reserved blocks
    uint8_t  padding[1024 - 84];    // Padding to 1024 bytes
} __attribute__((packed)) ext2_superblock_t;

// ext2 block group descriptor
typedef struct ext2_block_group_desc {
    uint32_t bg_block_bitmap;       // Block bitmap block
    uint32_t bg_inode_bitmap;       // Inode bitmap block
    uint32_t bg_inode_table;        // Inode table block
    uint16_t bg_free_blocks_count;  // Free blocks count
    uint16_t bg_free_inodes_count;  // Free inodes count
    uint16_t bg_used_dirs_count;    // Used directories count
    uint16_t bg_pad;                // Padding
    uint8_t  bg_reserved[12];       // Reserved
} __attribute__((packed)) ext2_block_group_desc_t;

// ext2 inode structure
typedef struct ext2_inode {
    uint16_t i_mode;                // File mode
    uint16_t i_uid;                 // Owner uid
    uint32_t i_size;                // File size in bytes
    uint32_t i_atime;               // Access time
    uint32_t i_ctime;               // Creation time
    uint32_t i_mtime;               // Modification time
    uint32_t i_dtime;               // Deletion time
    uint16_t i_gid;                 // Group id
    uint16_t i_links_count;         // Hard links count
    uint32_t i_blocks;              // Number of 512-byte blocks
    uint32_t i_flags;               // File flags
    uint32_t i_osd1;                // OS specific 1
    uint32_t i_block[15];           // Block pointers
    uint32_t i_generation;          // Generation number
    uint32_t i_file_acl;            // File ACL
    uint32_t i_dir_acl;             // Directory ACL
    uint32_t i_faddr;               // Fragment address
    uint8_t  i_osd2[12];            // OS specific 2
} __attribute__((packed)) ext2_inode_t;

// ext2 directory entry
typedef struct ext2_dir_entry {
    uint32_t inode;                 // Inode number
    uint16_t rec_len;               // Record length
    uint8_t  name_len;              // Name length
    uint8_t  file_type;             // File type
    char     name[];                // File name (variable length)
} __attribute__((packed)) ext2_dir_entry_t;

// ext2 directory entry file types
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

// Inode numbers
#define EXT2_BAD_INO        1   // Bad blocks inode
#define EXT2_ROOT_INO       2   // Root directory inode
#define EXT2_ACL_IDX_INO    3   // ACL index inode
#define EXT2_ACL_DATA_INO   4   // ACL data inode
#define EXT2_BOOT_LOADER_INO 5  // Boot loader inode
#define EXT2_UNDEL_DIR_INO  6   // Undelete directory inode
#define EXT2_FIRST_INO      11  // First non-reserved inode

// Utility macros
#define EXT2_BLOCK_SIZE(s) (1024 << (s)->s_log_block_size)
#define EXT2_ADDR_PER_BLOCK(s) (EXT2_BLOCK_SIZE(s) / sizeof(uint32_t))
#define EXT2_INODE_SIZE(s) (((s)->s_rev_level == 0) ? 128 : (s)->s_inode_size)

// --- Writable ext2 support: Expose fs struct and helpers for VFS ---

typedef struct ext2_fs {
    blockdev_t* device;
    struct ext2_superblock* superblock;
    struct ext2_block_group_desc* block_groups;
    uint32_t block_size;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t num_block_groups;
} ext2_fs_t;

// Writable helpers
/**
 * Allocate a free block.
 * @param fs Filesystem
 * @return Block number, or 0 on error
 */
uint32_t ext2_alloc_block(ext2_fs_t* fs);
/**
 * Free a block.
 * @param fs Filesystem
 * @param block_num Block number
 * @return 0 on success, -1 on error
 */
int ext2_free_block(ext2_fs_t* fs, uint32_t block_num);
/**
 * Allocate a free inode.
 * @param fs Filesystem
 * @return Inode number, or 0 on error
 */
uint32_t ext2_alloc_inode(ext2_fs_t* fs);
/**
 * Free an inode.
 * @param fs Filesystem
 * @param inode_num Inode number
 * @return 0 on success, -1 on error
 */
int ext2_free_inode(ext2_fs_t* fs, uint32_t inode_num);
/**
 * Write an inode to disk.
 * @param fs Filesystem
 * @param inode_num Inode number
 * @param inode Inode data
 * @return 0 on success, -1 on error
 */
int ext2_write_inode(ext2_fs_t* fs, uint32_t inode_num, const ext2_inode_t* inode);
/**
 * Add a directory entry.
 * @param fs Filesystem
 * @param dir_inode Directory inode
 * @param dir_inode_num Directory inode number
 * @param new_inode_num New entry inode number
 * @param name Entry name
 * @param type Entry type
 * @return 0 on success, -1 on error
 */
int ext2_add_dir_entry(ext2_fs_t* fs, struct ext2_inode* dir_inode, uint32_t dir_inode_num, uint32_t new_inode_num, const char* name, uint8_t type);
/**
 * Remove a directory entry by name.
 * @param fs Filesystem
 * @param dir_inode Directory inode
 * @param dir_inode_num Directory inode number
 * @param name Entry name
 * @return 0 on success, -1 on error
 */
int ext2_remove_dir_entry(ext2_fs_t* fs, struct ext2_inode* dir_inode, uint32_t dir_inode_num, const char* name);
/**
 * Write blocks to the device.
 * @param fs Filesystem
 * @param block Block number
 * @param count Number of blocks
 * @param buffer Data to write
 * @return 0 on success, -1 on error
 */
int ext2_write_blocks(ext2_fs_t* fs, uint32_t block, uint32_t count, const void* buffer);
/**
 * Read blocks from the device.
 * @param fs Filesystem
 * @param block Block number
 * @param count Number of blocks
 * @param buffer Output buffer
 * @return 0 on success, -1 on error
 */
int ext2_read_blocks(ext2_fs_t* fs, uint32_t block, uint32_t count, void* buffer);
/**
 * Read a symlink target from an inode.
 * @param fs Filesystem
 * @param inode Symlink inode
 * @param buffer Output buffer
 * @param max_len Buffer size
 * @return 0 on success, -1 on error
 */
int ext2_read_symlink(ext2_fs_t* fs, ext2_inode_t* inode, char* buffer, size_t max_len);
/**
 * Convert inode mode to a permission string (e.g., "rwxr-xr-x").
 * @param fs Filesystem (unused)
 * @param mode Inode mode
 * @param str Output string (11 bytes)
 */
void ext2_mode_to_string(ext2_fs_t* fs, uint16_t mode, char* str);
/**
 * Format a Unix timestamp as a human-readable string.
 * @param timestamp Unix timestamp
 * @param buffer Output buffer (at least 16 bytes)
 */
void ext2_format_time(uint32_t timestamp, char* buffer);
/**
 * Check if a directory is empty (only . and .. entries).
 * @param fs Filesystem
 * @param dir_inode Directory inode
 * @return 1 if empty, 0 if not, -1 on error
 */
int ext2_is_dir_empty(ext2_fs_t* fs, struct ext2_inode* dir_inode);

// Local string/mem helpers (used in VFS)
void* memcpy_local(void* dest, const void* src, size_t n);
void* memset_local(void* dest, int c, size_t n);

// Forward declarations
typedef struct ext2_fs ext2_fs_t;

// ext2 filesystem functions
/**
 * Mount an ext2 filesystem on the given block device.
 * @param device Block device
 * @return 0 on success, -1 on error
 */
int ext2_mount(blockdev_t* device);

/**
 * Get the global ext2_fs_t instance (mounted filesystem).
 * @return Pointer to ext2_fs_t, or NULL if not mounted
 */
ext2_fs_t* ext2_get_fs(void);

/**
 * Read an inode from disk.
 * @param fs Filesystem
 * @param inode_num Inode number (1-based)
 * @param inode Output inode
 * @return 0 on success, -1 on error
 */
int ext2_read_inode(ext2_fs_t* fs, uint32_t inode_num, ext2_inode_t* inode);

/**
 * Read file data from an inode.
 * @param fs Filesystem
 * @param inode Inode
 * @param offset Offset in bytes
 * @param size Number of bytes to read
 * @param buffer Output buffer
 * @return Number of bytes read, or -1 on error
 */
int ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, uint32_t offset, uint32_t size, void* buffer);

/**
 * Find a directory entry by name.
 * @param fs Filesystem
 * @param dir_inode Directory inode
 * @param name Entry name
 * @param out_inode Output inode number
 * @return 0 on success, -1 on error
 */
int ext2_find_dir_entry(ext2_fs_t* fs, ext2_inode_t* dir_inode, const char* name, uint32_t* out_inode);

/**
 * List directory contents (for shell ls command).
 * @param fs Filesystem
 * @param dir_inode Directory inode
 * @return 0 on success, -1 on error
 */
int ext2_list_dir(ext2_fs_t* fs, ext2_inode_t* dir_inode);

/**
 * Resolve a path to an inode number.
 * @param fs Filesystem
 * @param path Absolute path
 * @return Inode number, or 0 on error
 */
uint32_t ext2_path_to_inode(ext2_fs_t* fs, const char* path);

#endif // EXT2_H 