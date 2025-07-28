/*
 * ext2.c - ext2 Filesystem Implementation for NanOS
 *
 * Provides block device-backed ext2 filesystem support, including file and directory
 * operations, inode/block allocation, symlinks, permissions, and timestamps.
 * All public APIs are documented in include/ext2.h. Error codes: 0 = success, -1 = error.
 */
#include "ext2.h"
#include "blockdev.h"
#include "heap.h"
#include "monitor.h"
#include <stddef.h>
#include <stdint.h>

// Memory copy function
void* memcpy_local(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

// Memory set function
void* memset_local(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
    return dest;
}

// String comparison
static int strcmp_local(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

// String length
static size_t strlen_local(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// Read blocks from device
int ext2_read_blocks(ext2_fs_t* fs, uint32_t block, uint32_t count, void* buffer) {
    if (!fs || !fs->device || !buffer) return -1;
    
    // Convert filesystem blocks to device blocks
    uint32_t blocks_per_fs_block = fs->block_size / blockdev_get_block_size(fs->device);
    uint32_t device_block = block * blocks_per_fs_block;
    uint32_t device_count = count * blocks_per_fs_block;
    
    return blockdev_read(fs->device, device_block, device_count, buffer);
}

// Initialize ext2 filesystem
ext2_fs_t* ext2_init(blockdev_t* device) {
    if (!device) return NULL;
    
    ext2_fs_t* fs = (ext2_fs_t*)kmalloc(sizeof(ext2_fs_t));
    if (!fs) return NULL;
    
    memset_local(fs, 0, sizeof(ext2_fs_t));
    fs->device = device;
    
    // Read superblock
    fs->superblock = (ext2_superblock_t*)kmalloc(sizeof(ext2_superblock_t));
    if (!fs->superblock) {
        kfree(fs);
        return NULL;
    }
    
    // Read superblock from offset 1024
    uint32_t superblock_block = EXT2_SUPERBLOCK_OFFSET / blockdev_get_block_size(device);
    if (blockdev_read(device, superblock_block, 2, fs->superblock) != 0) {
        kfree(fs->superblock);
        kfree(fs);
        return NULL;
    }
    
    // Verify magic number
    if (fs->superblock->s_magic != EXT2_MAGIC) {
        monitor_write("[EXT2] Invalid magic number: 0x");
        monitor_write_hex(fs->superblock->s_magic);
        monitor_write("\n");
        kfree(fs->superblock);
        kfree(fs);
        return NULL;
    }
    
    // Calculate filesystem parameters
    fs->block_size = EXT2_BLOCK_SIZE(fs->superblock);
    fs->blocks_per_group = fs->superblock->s_blocks_per_group;
    fs->inodes_per_group = fs->superblock->s_inodes_per_group;
    fs->num_block_groups = (fs->superblock->s_blocks_count + fs->blocks_per_group - 1) / fs->blocks_per_group;
    
    monitor_write("[EXT2] Initialized filesystem:\n");
    monitor_write("  Block size: ");
    monitor_write_dec(fs->block_size);
    monitor_write("\n  Total blocks: ");
    monitor_write_dec(fs->superblock->s_blocks_count);
    monitor_write("\n  Block groups: ");
    monitor_write_dec(fs->num_block_groups);
    monitor_write("\n");
    
    // Read block group descriptors
    uint32_t bgd_blocks = (fs->num_block_groups * sizeof(ext2_block_group_desc_t) + fs->block_size - 1) / fs->block_size;
    fs->block_groups = (ext2_block_group_desc_t*)kmalloc(bgd_blocks * fs->block_size);
    if (!fs->block_groups) {
        kfree(fs->superblock);
        kfree(fs);
        return NULL;
    }
    
    uint32_t bgd_block = fs->superblock->s_first_data_block + 1;
    if (ext2_read_blocks(fs, bgd_block, bgd_blocks, fs->block_groups) != 0) {
        kfree(fs->block_groups);
        kfree(fs->superblock);
        kfree(fs);
        return NULL;
    }
    
    return fs;
}

// Read inode
int ext2_read_inode(ext2_fs_t* fs, uint32_t inode_num, ext2_inode_t* inode) {
    if (!fs || !inode || inode_num == 0) return -1;
    
    // Calculate block group and offset
    uint32_t group = (inode_num - 1) / fs->inodes_per_group;
    uint32_t offset = (inode_num - 1) % fs->inodes_per_group;
    
    if (group >= fs->num_block_groups) return -1;
    
    // Get inode table block
    uint32_t inode_table_block = fs->block_groups[group].bg_inode_table;
    uint32_t inode_size = sizeof(ext2_inode_t);
    uint32_t inodes_per_block = fs->block_size / inode_size;
    
    uint32_t block_offset = offset / inodes_per_block;
    uint32_t inode_offset = offset % inodes_per_block;
    
    // Read the block containing the inode
    void* block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) return -1;
    
    if (ext2_read_blocks(fs, inode_table_block + block_offset, 1, block_buffer) != 0) {
        kfree(block_buffer);
        return -1;
    }
    
    // Copy inode data
    ext2_inode_t* inodes = (ext2_inode_t*)block_buffer;
    memcpy_local(inode, &inodes[inode_offset], sizeof(ext2_inode_t));
    
    kfree(block_buffer);
    return 0;
}

// Read data block from inode
static int ext2_read_data_block(ext2_fs_t* fs, ext2_inode_t* inode, uint32_t block_index, void* buffer) {
    if (!fs || !inode || !buffer) return -1;
    
    uint32_t block_num = 0;
    
    // Direct blocks (0-11)
    if (block_index < 12) {
        block_num = inode->i_block[block_index];
    }
    // Single indirect block (12)
    else if (block_index < 12 + (fs->block_size / 4)) {
        uint32_t indirect_block = inode->i_block[12];
        if (indirect_block == 0) return -1;
        
        void* indirect_buffer = kmalloc(fs->block_size);
        if (!indirect_buffer) return -1;
        
        if (ext2_read_blocks(fs, indirect_block, 1, indirect_buffer) != 0) {
            kfree(indirect_buffer);
            return -1;
        }
        
        uint32_t* indirect_blocks = (uint32_t*)indirect_buffer;
        block_num = indirect_blocks[block_index - 12];
        kfree(indirect_buffer);
    }
    // Double indirect not implemented yet
    else {
        return -1;
    }
    
    if (block_num == 0) return -1;
    
    return ext2_read_blocks(fs, block_num, 1, buffer);
}

// Read file data
int ext2_read_file(ext2_fs_t* fs, ext2_inode_t* inode, uint32_t offset, uint32_t size, void* buffer) {
    if (!fs || !inode || !buffer) return -1;
    if (offset >= inode->i_size) return 0;
    
    uint32_t file_size = inode->i_size;
    if (offset + size > file_size) {
        size = file_size - offset;
    }
    
    uint32_t bytes_read = 0;
    uint8_t* out_buffer = (uint8_t*)buffer;
    
    while (bytes_read < size) {
        uint32_t block_index = (offset + bytes_read) / fs->block_size;
        uint32_t block_offset = (offset + bytes_read) % fs->block_size;
        uint32_t bytes_to_read = fs->block_size - block_offset;
        
        if (bytes_to_read > size - bytes_read) {
            bytes_to_read = size - bytes_read;
        }
        
        void* block_buffer = kmalloc(fs->block_size);
        if (!block_buffer) return -1;
        
        if (ext2_read_data_block(fs, inode, block_index, block_buffer) != 0) {
            kfree(block_buffer);
            return -1;
        }
        
        memcpy_local(out_buffer + bytes_read, (uint8_t*)block_buffer + block_offset, bytes_to_read);
        
        kfree(block_buffer);
        bytes_read += bytes_to_read;
    }
    
    return bytes_read;
}

// Find directory entry
int ext2_find_dir_entry(ext2_fs_t* fs, ext2_inode_t* dir_inode, const char* name, uint32_t* out_inode) {
    if (!fs || !dir_inode || !name || !out_inode) return -1;
    if ((dir_inode->i_mode & EXT2_S_IFDIR) == 0) return -1;
    
    uint32_t dir_size = dir_inode->i_size;
    void* dir_buffer = kmalloc(dir_size);
    if (!dir_buffer) return -1;
    
    if (ext2_read_file(fs, dir_inode, 0, dir_size, dir_buffer) != (int)dir_size) {
        kfree(dir_buffer);
        return -1;
    }
    
    uint32_t offset = 0;
    while (offset < dir_size) {
        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)((uint8_t*)dir_buffer + offset);
        
        if (entry->rec_len == 0) break;
        
        if (entry->inode != 0 && entry->name_len == strlen_local(name)) {
            char entry_name[256];
            memcpy_local(entry_name, entry->name, entry->name_len);
            entry_name[entry->name_len] = '\0';
            
            if (strcmp_local(entry_name, name) == 0) {
                *out_inode = entry->inode;
                kfree(dir_buffer);
                return 0;
            }
        }
        
        offset += entry->rec_len;
    }
    
    kfree(dir_buffer);
    return -1;
}

// List directory contents
int ext2_list_dir(ext2_fs_t* fs, ext2_inode_t* dir_inode) {
    if (!fs || !dir_inode) return -1;
    if ((dir_inode->i_mode & EXT2_S_IFDIR) == 0) return -1;
    
    uint32_t dir_size = dir_inode->i_size;
    void* dir_buffer = kmalloc(dir_size);
    if (!dir_buffer) return -1;
    
    if (ext2_read_file(fs, dir_inode, 0, dir_size, dir_buffer) != (int)dir_size) {
        kfree(dir_buffer);
        return -1;
    }
    
    uint32_t offset = 0;
    while (offset < dir_size) {
        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)((uint8_t*)dir_buffer + offset);
        
        if (entry->rec_len == 0) break;
        
        if (entry->inode != 0) {
            char entry_name[256];
            memcpy_local(entry_name, entry->name, entry->name_len);
            entry_name[entry->name_len] = '\0';
            
            // Get file type
            ext2_inode_t inode;
            if (ext2_read_inode(fs, entry->inode, &inode) == 0) {
                if (inode.i_mode & EXT2_S_IFDIR) {
                    monitor_write("[DIR]  ");
                } else {
                    monitor_write("[FILE] ");
                }
                monitor_write(entry_name);
                monitor_write(" (");
                monitor_write_dec(inode.i_size);
                monitor_write(" bytes)\n");
            }
        }
        
        offset += entry->rec_len;
    }
    
    kfree(dir_buffer);
    return 0;
}

// Read symlink target
int ext2_read_symlink(ext2_fs_t* fs, ext2_inode_t* inode, char* buffer, size_t max_len) {
    if (!fs || !inode || !buffer || max_len == 0) return -1;
    if ((inode->i_mode & EXT2_S_IFLNK) != EXT2_S_IFLNK) return -1;
    uint32_t link_len = inode->i_size;
    if (link_len >= max_len) link_len = max_len - 1;
    if (link_len <= 60) {
        memcpy_local(buffer, (char*)inode->i_block, link_len);
        buffer[link_len] = '\0';
        return 0;
    }
    if (ext2_read_file(fs, inode, 0, link_len, buffer) != (int)link_len) {
        return -1;
    }
    buffer[link_len] = '\0';
    return 0;
}

void ext2_mode_to_string(ext2_fs_t* fs, uint16_t mode, char* str) {
    (void)fs;
    if ((mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) str[0] = 'd';
    else if ((mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) str[0] = 'l';
    else str[0] = '-';
    str[1] = (mode & 0400) ? 'r' : '-';
    str[2] = (mode & 0200) ? 'w' : '-';
    str[3] = (mode & 0100) ? 'x' : '-';
    str[4] = (mode & 040) ? 'r' : '-';
    str[5] = (mode & 020) ? 'w' : '-';
    str[6] = (mode & 010) ? 'x' : '-';
    str[7] = (mode & 04) ? 'r' : '-';
    str[8] = (mode & 02) ? 'w' : '-';
    str[9] = (mode & 01) ? 'x' : '-';
    str[10] = '\0';
}

void ext2_format_time(uint32_t timestamp, char* buffer) {
    uint32_t seconds = timestamp;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    if (days > 0) {
        int len = 0;
        uint32_t temp = days;
        while (temp > 0) { len++; temp /= 10; }
        for (int i = len - 1; i >= 0; i--) {
            buffer[i] = '0' + (days % 10);
            days /= 10;
        }
        buffer[len] = 'd';
        buffer[len + 1] = ' ';
        buffer += len + 2;
    }
    buffer[0] = '0' + ((hours % 24) / 10);
    buffer[1] = '0' + ((hours % 24) % 10);
    buffer[2] = ':';
    buffer[3] = '0' + ((minutes % 60) / 10);
    buffer[4] = '0' + ((minutes % 60) % 10);
    buffer[5] = ':';
    buffer[6] = '0' + ((seconds % 60) / 10);
    buffer[7] = '0' + ((seconds % 60) % 10);
    buffer[8] = '\0';
}

int ext2_is_dir_empty(ext2_fs_t* fs, ext2_inode_t* dir_inode) {
    if (!fs || !dir_inode) return -1;
    if ((dir_inode->i_mode & EXT2_S_IFDIR) == 0) return -1;
    uint32_t dir_size = dir_inode->i_size;
    void* dir_buffer = kmalloc(dir_size);
    if (!dir_buffer) return -1;
    if (ext2_read_file(fs, dir_inode, 0, dir_size, dir_buffer) != (int)dir_size) {
        kfree(dir_buffer);
        return -1;
    }
    uint32_t offset = 0;
    int entry_count = 0;
    while (offset < dir_size) {
        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)((uint8_t*)dir_buffer + offset);
        if (entry->rec_len == 0) break;
        if (entry->inode != 0) {
            char entry_name[256];
            memcpy_local(entry_name, entry->name, entry->name_len);
            entry_name[entry->name_len] = '\0';
            if (strcmp_local(entry_name, ".") != 0 && strcmp_local(entry_name, "..") != 0) {
                entry_count++;
            }
        }
        offset += entry->rec_len;
    }
    kfree(dir_buffer);
    return (entry_count == 0) ? 1 : 0;
}

uint32_t ext2_path_to_inode(ext2_fs_t* fs, const char* path) {
    if (!fs || !path) return 0;
    uint32_t current_inode = EXT2_ROOT_INO;
    if (strcmp_local(path, "/") == 0) {
        return EXT2_ROOT_INO;
    }
    if (path[0] == '/') path++;
    char component[256];
    int comp_idx = 0;
    while (*path) {
        if (*path == '/') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';
                ext2_inode_t dir_inode;
                if (ext2_read_inode(fs, current_inode, &dir_inode) != 0) {
                    return 0;
                }
                if (ext2_find_dir_entry(fs, &dir_inode, component, &current_inode) != 0) {
                    return 0;
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
    if (comp_idx > 0) {
        component[comp_idx] = '\0';
        ext2_inode_t dir_inode;
        if (ext2_read_inode(fs, current_inode, &dir_inode) != 0) {
            return 0;
        }
        if (ext2_find_dir_entry(fs, &dir_inode, component, &current_inode) != 0) {
            return 0;
        }
    }
    return current_inode;
}

// Writable ext2 helpers (must be non-static)

int ext2_write_blocks(ext2_fs_t* fs, uint32_t block, uint32_t count, const void* buffer) {
    if (!fs || !fs->device || !buffer) return -1;
    uint32_t blocks_per_fs_block = fs->block_size / blockdev_get_block_size(fs->device);
    uint32_t device_block = block * blocks_per_fs_block;
    uint32_t device_count = count * blocks_per_fs_block;
    return blockdev_write(fs->device, device_block, device_count, buffer);
}

/**
 * Allocate a free block in the ext2 filesystem.
 * @param fs Filesystem
 * @return Block number, or 0 on error
 */
uint32_t ext2_alloc_block(ext2_fs_t* fs) {
    for (uint32_t group = 0; group < fs->num_block_groups; ++group) {
        uint8_t* bitmap = (uint8_t*)kmalloc(fs->block_size);
        if (!bitmap) continue;
        if (ext2_read_blocks(fs, fs->block_groups[group].bg_block_bitmap, 1, bitmap) != 0) { kfree(bitmap); continue; }
        for (uint32_t i = 0; i < fs->blocks_per_group; ++i) {
            uint32_t byte = i / 8, bit = i % 8;
            if (!(bitmap[byte] & (1 << bit))) {
                bitmap[byte] |= (1 << bit);
                if (ext2_write_blocks(fs, fs->block_groups[group].bg_block_bitmap, 1, bitmap) != 0) { kfree(bitmap); continue; }
                kfree(bitmap);
                fs->block_groups[group].bg_free_blocks_count--;
                fs->superblock->s_free_blocks_count--;
                // TODO: Write back group desc and superblock
                return group * fs->blocks_per_group + i + fs->superblock->s_first_data_block;
            }
        }
        kfree(bitmap);
    }
    return 0;
}

int ext2_free_block(ext2_fs_t* fs, uint32_t block_num) {
    if (!fs || block_num < fs->superblock->s_first_data_block) return -1;
    uint32_t rel = block_num - fs->superblock->s_first_data_block;
    uint32_t group = rel / fs->blocks_per_group;
    uint32_t index = rel % fs->blocks_per_group;
    uint8_t* bitmap = (uint8_t*)kmalloc(fs->block_size);
    if (!bitmap) return -1;
    if (ext2_read_blocks(fs, fs->block_groups[group].bg_block_bitmap, 1, bitmap) != 0) { kfree(bitmap); return -1; }
    uint32_t byte = index / 8, bit = index % 8;
    bitmap[byte] &= ~(1 << bit);
    int res = ext2_write_blocks(fs, fs->block_groups[group].bg_block_bitmap, 1, bitmap);
    kfree(bitmap);
    if (res == 0) {
        fs->block_groups[group].bg_free_blocks_count++;
        fs->superblock->s_free_blocks_count++;
        // TODO: Write back group desc and superblock
    }
    return res;
}

uint32_t ext2_alloc_inode(ext2_fs_t* fs) {
    for (uint32_t group = 0; group < fs->num_block_groups; ++group) {
        uint8_t* bitmap = (uint8_t*)kmalloc(fs->block_size);
        if (!bitmap) continue;
        if (ext2_read_blocks(fs, fs->block_groups[group].bg_inode_bitmap, 1, bitmap) != 0) { kfree(bitmap); continue; }
        for (uint32_t i = 0; i < fs->inodes_per_group; ++i) {
            uint32_t byte = i / 8, bit = i % 8;
            if (!(bitmap[byte] & (1 << bit))) {
                bitmap[byte] |= (1 << bit);
                if (ext2_write_blocks(fs, fs->block_groups[group].bg_inode_bitmap, 1, bitmap) != 0) { kfree(bitmap); continue; }
                kfree(bitmap);
                fs->block_groups[group].bg_free_inodes_count--;
                fs->superblock->s_free_inodes_count--;
                // TODO: Write back group desc and superblock
                return group * fs->inodes_per_group + i + 1;
            }
        }
        kfree(bitmap);
    }
    return 0;
}

int ext2_free_inode(ext2_fs_t* fs, uint32_t inode_num) {
    if (!fs || inode_num == 0) return -1;
    uint32_t group = (inode_num - 1) / fs->inodes_per_group;
    uint32_t index = (inode_num - 1) % fs->inodes_per_group;
    uint8_t* bitmap = (uint8_t*)kmalloc(fs->block_size);
    if (!bitmap) return -1;
    if (ext2_read_blocks(fs, fs->block_groups[group].bg_inode_bitmap, 1, bitmap) != 0) { kfree(bitmap); return -1; }
    uint32_t byte = index / 8, bit = index % 8;
    bitmap[byte] &= ~(1 << bit);
    int res = ext2_write_blocks(fs, fs->block_groups[group].bg_inode_bitmap, 1, bitmap);
    kfree(bitmap);
    if (res == 0) {
        fs->block_groups[group].bg_free_inodes_count++;
        fs->superblock->s_free_inodes_count++;
        // TODO: Write back group desc and superblock
    }
    return res;
}

int ext2_write_inode(ext2_fs_t* fs, uint32_t inode_num, const ext2_inode_t* inode) {
    if (!fs || !inode || inode_num == 0) return -1;
    uint32_t group = (inode_num - 1) / fs->inodes_per_group;
    uint32_t offset = (inode_num - 1) % fs->inodes_per_group;
    if (group >= fs->num_block_groups) return -1;
    uint32_t inode_table_block = fs->block_groups[group].bg_inode_table;
    uint32_t inode_size = sizeof(ext2_inode_t);
    uint32_t inodes_per_block = fs->block_size / inode_size;
    uint32_t block_offset = offset / inodes_per_block;
    uint32_t inode_offset = offset % inodes_per_block;
    void* block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) return -1;
    if (ext2_read_blocks(fs, inode_table_block + block_offset, 1, block_buffer) != 0) {
        kfree(block_buffer);
        return -1;
    }
    ext2_inode_t* inodes = (ext2_inode_t*)block_buffer;
    memcpy_local(&inodes[inode_offset], inode, sizeof(ext2_inode_t));
    int res = ext2_write_blocks(fs, inode_table_block + block_offset, 1, block_buffer);
    kfree(block_buffer);
    return res;
}

int ext2_add_dir_entry(ext2_fs_t* fs, ext2_inode_t* dir_inode, uint32_t dir_inode_num, uint32_t new_inode_num, const char* name, uint8_t type) {
    if (!fs || !dir_inode || !name || new_inode_num == 0) return -1;
    uint32_t name_len = strlen_local(name);
    if (name_len == 0 || name_len > 255) return -1;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t block_count = (dir_size + fs->block_size - 1) / fs->block_size;
    ext2_dir_entry_t* entry = NULL;
    uint8_t* block = (uint8_t*)kmalloc(fs->block_size);
    if (!block) return -1;
    for (uint32_t b = 0; b < block_count; ++b) {
        if (ext2_read_data_block(fs, dir_inode, b, block) != 0) continue;
        uint32_t offset = 0;
        while (offset < fs->block_size) {
            entry = (ext2_dir_entry_t*)(block + offset);
            if (entry->rec_len == 0) break;
            uint32_t actual_len = 8 + ((entry->name_len + 3) & ~3);
            if (entry->rec_len > actual_len) {
                uint16_t new_rec_len = entry->rec_len - actual_len;
                entry->rec_len = actual_len;
                ext2_dir_entry_t* new_entry = (ext2_dir_entry_t*)(block + offset + actual_len);
                new_entry->inode = new_inode_num;
                new_entry->rec_len = new_rec_len;
                new_entry->name_len = name_len;
                new_entry->file_type = type;
                memcpy_local(new_entry->name, name, name_len);
                int res = ext2_write_blocks(fs, dir_inode->i_block[b], 1, block);
                kfree(block);
                return res;
            }
            offset += entry->rec_len;
        }
    }
    uint32_t new_block = ext2_alloc_block(fs);
    if (new_block == 0) { kfree(block); return -1; }
    memset_local(block, 0, fs->block_size);
    entry = (ext2_dir_entry_t*)block;
    entry->inode = new_inode_num;
    entry->rec_len = fs->block_size;
    entry->name_len = name_len;
    entry->file_type = type;
    memcpy_local(entry->name, name, name_len);
    for (uint32_t i = 0; i < 12; ++i) {
        if (dir_inode->i_block[i] == 0) {
            dir_inode->i_block[i] = new_block;
            break;
        }
    }
    dir_inode->i_size += fs->block_size;
    int res = ext2_write_blocks(fs, new_block, 1, block);
    kfree(block);
    ext2_write_inode(fs, dir_inode_num, dir_inode);
    return res;
}

int ext2_remove_dir_entry(ext2_fs_t* fs, ext2_inode_t* dir_inode, uint32_t dir_inode_num, const char* name) {
    if (!fs || !dir_inode || !name) return -1;
    uint32_t name_len = strlen_local(name);
    if (name_len == 0 || name_len > 255) return -1;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t block_count = (dir_size + fs->block_size - 1) / fs->block_size;
    ext2_dir_entry_t* entry = NULL;
    uint8_t* block = (uint8_t*)kmalloc(fs->block_size);
    if (!block) return -1;
    for (uint32_t b = 0; b < block_count; ++b) {
        if (ext2_read_data_block(fs, dir_inode, b, block) != 0) continue;
        uint32_t offset = 0, prev_offset = 0;
        ext2_dir_entry_t* prev_entry = NULL;
        while (offset < fs->block_size) {
            entry = (ext2_dir_entry_t*)(block + offset);
            if (entry->rec_len == 0) break;
            if (entry->inode != 0 && entry->name_len == name_len && strcmp_local(entry->name, name) == 0) {
                if (prev_entry) {
                    prev_entry->rec_len += entry->rec_len;
                } else {
                    entry->inode = 0;
                }
                int res = ext2_write_blocks(fs, dir_inode->i_block[b], 1, block);
                kfree(block);
                return res;
            }
            prev_offset = offset;
            prev_entry = entry;
            offset += entry->rec_len;
        }
    }
    kfree(block);
    return -1;
}

// Get filesystem instance
static ext2_fs_t* global_fs = NULL;

int ext2_mount(blockdev_t* device) {
    if (global_fs) return -1; // Already mounted
    
    global_fs = ext2_init(device);
    if (!global_fs) return -1;
    
    monitor_write("[EXT2] Filesystem mounted successfully\n");
    return 0;
}

ext2_fs_t* ext2_get_fs(void) {
    return global_fs;
} 