#include "blockdev.h"
#include "heap.h"
#include "monitor.h"
#include <stddef.h>
#include <stdint.h>

#define RAMDISK_BLOCK_SIZE 512
#define RAMDISK_DEFAULT_SIZE (256 * 1024) // 256KB for ext2 filesystem

typedef struct ramdisk_data {
    uint8_t* data;
    uint32_t size;
    uint32_t block_count;
} ramdisk_data_t;

// Memory copy function
static void memcpy_local(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// Memory set function
static void memset_local(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
}

static int ramdisk_read(void* dev_data, uint32_t block, uint32_t count, void* buffer) {
    ramdisk_data_t* ramdisk = (ramdisk_data_t*)dev_data;
    
    if (!ramdisk || !ramdisk->data || !buffer) return -1;
    if (block >= ramdisk->block_count) return -1;
    if (block + count > ramdisk->block_count) return -1;
    
    uint32_t offset = block * RAMDISK_BLOCK_SIZE;
    uint32_t bytes = count * RAMDISK_BLOCK_SIZE;
    
    memcpy_local(buffer, ramdisk->data + offset, bytes);
    return 0;
}

static int ramdisk_write(void* dev_data, uint32_t block, uint32_t count, const void* buffer) {
    ramdisk_data_t* ramdisk = (ramdisk_data_t*)dev_data;
    
    if (!ramdisk || !ramdisk->data || !buffer) return -1;
    if (block >= ramdisk->block_count) return -1;
    if (block + count > ramdisk->block_count) return -1;
    
    uint32_t offset = block * RAMDISK_BLOCK_SIZE;
    uint32_t bytes = count * RAMDISK_BLOCK_SIZE;
    
    memcpy_local(ramdisk->data + offset, buffer, bytes);
    return 0;
}

static uint32_t ramdisk_get_block_count(void* dev_data) {
    ramdisk_data_t* ramdisk = (ramdisk_data_t*)dev_data;
    return ramdisk ? ramdisk->block_count : 0;
}

static uint32_t ramdisk_get_block_size(void* dev_data) {
    return RAMDISK_BLOCK_SIZE;
}

static blockdev_ops_t ramdisk_ops = {
    .read = ramdisk_read,
    .write = ramdisk_write,
    .get_block_count = ramdisk_get_block_count,
    .get_block_size = ramdisk_get_block_size
};

blockdev_t* ramdisk_create(const char* name, uint32_t size) {
    // Allocate ramdisk data structure
    ramdisk_data_t* ramdisk = (ramdisk_data_t*)kmalloc(sizeof(ramdisk_data_t));
    if (!ramdisk) return NULL;
    
    // Round size up to block boundary
    if (size == 0) size = RAMDISK_DEFAULT_SIZE;
    size = (size + RAMDISK_BLOCK_SIZE - 1) & ~(RAMDISK_BLOCK_SIZE - 1);
    
    // Allocate storage
    ramdisk->data = (uint8_t*)kmalloc(size);
    if (!ramdisk->data) {
        kfree(ramdisk);
        return NULL;
    }
    
    // Initialize ramdisk
    memset_local(ramdisk->data, 0, size);
    ramdisk->size = size;
    ramdisk->block_count = size / RAMDISK_BLOCK_SIZE;
    
    // Allocate block device structure
    blockdev_t* dev = (blockdev_t*)kmalloc(sizeof(blockdev_t));
    if (!dev) {
        kfree(ramdisk->data);
        kfree(ramdisk);
        return NULL;
    }
    
    // Initialize block device
    dev->name = name;
    dev->device_data = ramdisk;
    dev->ops = &ramdisk_ops;
    dev->block_count = ramdisk->block_count;
    dev->block_size = RAMDISK_BLOCK_SIZE;
    
    return dev;
}

void ramdisk_destroy(blockdev_t* dev) {
    if (!dev) return;
    
    ramdisk_data_t* ramdisk = (ramdisk_data_t*)dev->device_data;
    if (ramdisk) {
        if (ramdisk->data) {
            kfree(ramdisk->data);
        }
        kfree(ramdisk);
    }
    
    kfree(dev);
}

// Load ext2 filesystem image into ramdisk
int ramdisk_load_ext2_image(blockdev_t* dev, const void* image_data, uint32_t image_size) {
    if (!dev || !image_data || image_size == 0) return -1;
    
    ramdisk_data_t* ramdisk = (ramdisk_data_t*)dev->device_data;
    if (!ramdisk || !ramdisk->data) return -1;
    
    if (image_size > ramdisk->size) return -1;
    
    memcpy_local(ramdisk->data, image_data, image_size);
    return 0;
} 