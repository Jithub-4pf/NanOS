#include "blockdev.h"
#include "heap.h"
#include "monitor.h"
#include <stddef.h>

#define MAX_BLOCK_DEVICES 16

static blockdev_t* block_devices[MAX_BLOCK_DEVICES];
static int device_count = 0;

// Simple string comparison
static int str_cmp(const char* a, const char* b) {
    if (!a || !b) return -1;
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

int blockdev_read(blockdev_t* dev, uint32_t block, uint32_t count, void* buffer) {
    if (!dev || !dev->ops || !dev->ops->read) return -1;
    return dev->ops->read(dev->device_data, block, count, buffer);
}

int blockdev_write(blockdev_t* dev, uint32_t block, uint32_t count, const void* buffer) {
    if (!dev || !dev->ops || !dev->ops->write) return -1;
    return dev->ops->write(dev->device_data, block, count, buffer);
}

uint32_t blockdev_get_block_count(blockdev_t* dev) {
    if (!dev) return 0;
    if (dev->ops && dev->ops->get_block_count) {
        return dev->ops->get_block_count(dev->device_data);
    }
    return dev->block_count;
}

uint32_t blockdev_get_block_size(blockdev_t* dev) {
    if (!dev) return 0;
    if (dev->ops && dev->ops->get_block_size) {
        return dev->ops->get_block_size(dev->device_data);
    }
    return dev->block_size;
}

int blockdev_register(blockdev_t* dev) {
    if (!dev || device_count >= MAX_BLOCK_DEVICES) return -1;
    
    // Check if device with same name already exists
    for (int i = 0; i < device_count; i++) {
        if (str_cmp(block_devices[i]->name, dev->name) == 0) {
            return -1; // Device already exists
        }
    }
    
    block_devices[device_count] = dev;
    device_count++;
    
    monitor_write("[BLOCKDEV] Registered device: ");
    monitor_write(dev->name);
    monitor_write("\n");
    
    return 0;
}

blockdev_t* blockdev_get(const char* name) {
    if (!name) return NULL;
    
    for (int i = 0; i < device_count; i++) {
        if (str_cmp(block_devices[i]->name, name) == 0) {
            return block_devices[i];
        }
    }
    
    return NULL;
} 