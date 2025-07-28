#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_SIZE 512

// Block device operations
typedef struct blockdev_ops {
    int (*read)(void* dev, uint32_t block, uint32_t count, void* buffer);
    int (*write)(void* dev, uint32_t block, uint32_t count, const void* buffer);
    uint32_t (*get_block_count)(void* dev);
    uint32_t (*get_block_size)(void* dev);
} blockdev_ops_t;

// Block device structure
typedef struct blockdev {
    const char* name;
    void* device_data;
    blockdev_ops_t* ops;
    uint32_t block_count;
    uint32_t block_size;
} blockdev_t;

// Block device interface functions
int blockdev_read(blockdev_t* dev, uint32_t block, uint32_t count, void* buffer);
int blockdev_write(blockdev_t* dev, uint32_t block, uint32_t count, const void* buffer);
uint32_t blockdev_get_block_count(blockdev_t* dev);
uint32_t blockdev_get_block_size(blockdev_t* dev);

// Block device registry
int blockdev_register(blockdev_t* dev);
blockdev_t* blockdev_get(const char* name);

#endif // BLOCKDEV_H 