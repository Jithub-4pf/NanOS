#ifndef RAMDISK_H
#define RAMDISK_H

#include "blockdev.h"
#include <stdint.h>

// Create a new ramdisk
blockdev_t* ramdisk_create(const char* name, uint32_t size);

// Destroy a ramdisk
void ramdisk_destroy(blockdev_t* dev);

// Load ext2 filesystem image into ramdisk
int ramdisk_load_ext2_image(blockdev_t* dev, const void* image_data, uint32_t image_size);

#endif // RAMDISK_H 