#!/bin/bash

# Create a small ext2 filesystem image for NanOS
echo "Creating ext2 filesystem image..."

# Create a 256KB image file (minimum for ext2)
dd if=/dev/zero of=test_fs.img bs=1024 count=256

# Format it as ext2
mkfs.ext2 -F test_fs.img

# Create a temporary mount point
mkdir -p mnt

# Mount the image
sudo mount -o loop test_fs.img mnt

# Create some test files (using sudo since filesystem is owned by root)
sudo bash -c 'echo "Hello, NanOS filesystem!" > mnt/hello.txt'
sudo bash -c 'echo "This is a test file for the ext2 filesystem implementation." > mnt/readme.txt'
sudo bash -c 'echo "Welcome to NanOS" > mnt/welcome.txt'

# Create a test directory
sudo mkdir mnt/testdir
sudo bash -c 'echo "File in directory" > mnt/testdir/test.txt'

# Create a larger test file
sudo bash -c 'echo "This is line 1" > mnt/multiline.txt'
sudo bash -c 'echo "This is line 2" >> mnt/multiline.txt'
sudo bash -c 'echo "This is line 3" >> mnt/multiline.txt'
sudo bash -c 'echo "End of file" >> mnt/multiline.txt'

# Show what we created
echo "Created files:"
ls -la mnt/

# Unmount
sudo umount mnt
rmdir mnt

echo "ext2 image created: test_fs.img"
echo "Converting to C array..."

# Convert to C array
xxd -i test_fs.img > ext2_image.h
echo "C header created: ext2_image.h"

echo "Done!" 