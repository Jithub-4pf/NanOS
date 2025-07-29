# NanOS - A Modern Operating System Built From Scratch

NanOS is a complete 32-bit operating system that I built from the ground up as a passion project in systems programming. What started as curiosity about how computers really work became a fully functional OS featuring a production-quality EXT2 filesystem, memory management with paging, and a comprehensive shell interface—all implemented without using existing operating system code.

## Why I Built This

I've always been fascinated by the lowest levels of computing: how does a computer transform electricity into the applications we use every day? Building an operating system from scratch gave me the opportunity to understand every layer of the stack, from bootloaders and hardware interrupts to filesystems and user interfaces. There's something incredibly satisfying about writing code that runs directly on bare metal, with no underlying operating system to rely on.

This project represents hundreds of hours of research, debugging, and problem-solving. Every feature had to be implemented from first principles, teaching me not just how operating systems work, but how to tackle complex technical challenges systematically.

## Key Features

**Complete Filesystem Support**: NanOS implements a full EXT2 filesystem—the same format used by early Linux systems. This means it can create, read, write, and delete files and directories, manage file permissions, and handle symbolic links. The filesystem is built on a modular architecture with separate layers for block devices, storage drivers, and the virtual filesystem (VFS).

**Memory Management**: The system includes sophisticated memory management with paging support, allowing it to efficiently allocate and protect memory regions. It features a custom heap allocator and can dynamically map new memory pages as needed.

**Hardware Interaction**: NanOS directly interfaces with computer hardware, including keyboard input, display output, and storage devices. It implements its own interrupt handlers and device drivers without relying on existing operating system services.

**Production-Quality Shell**: The included command-line interface supports 22 different commands, from basic file operations (`ls`, `cat`, `mkdir`) to system utilities (`ps`, `meminfo`, `chmod`). It includes features like file redirection and comprehensive error handling.

**Robust Architecture**: The codebase follows professional software engineering practices with modular design, clear separation of concerns, and extensive documentation. Each component (memory management, filesystem, drivers) is independently testable and maintainable.

## Quick Start Guide

Want to see NanOS in action? You can run it using the QEMU virtual machine software:

### Install QEMU

**On macOS:**
```bash
# Using Homebrew
brew install qemu
```

**On Debian/Ubuntu:**
```bash
# Update package list and install QEMU
sudo apt update
sudo apt install qemu-system-x86
```

### Run NanOS

1. Download the latest `myos.bin` file from the releases section
2. Run NanOS in QEMU:
```bash
qemu-system-i386 -kernel myos.bin
```

You'll see the boot process followed by a shell prompt where you can try commands like:
- `help` - See all available commands
- `ls` - List files in the current directory
- `cat filename` - Display file contents
- `mkdir testdir` - Create a new directory
- `echo "Hello World" > test.txt` - Create a file with content

The system boots with a small filesystem containing example files to explore!

## Technical Deep Dive

For those interested in the implementation details:

- **Bootloader Integration**: Uses the Multiboot standard to boot via GRUB
- **Memory Architecture**: 32-bit protected mode with identity mapping and demand paging
- **Interrupt System**: Custom interrupt descriptor table (IDT) with handlers for hardware and software interrupts  
- **Storage Stack**: Block device abstraction layer supporting multiple storage types
- **Filesystem Implementation**: Complete EXT2 driver supporting inodes, directories, and file operations
- **Device Drivers**: PS/2 keyboard driver with scancode translation and input buffering

## Future Development

I'm continuously expanding NanOS with new features:

- **Persistent Storage**: Currently implementing ATA/IDE drivers for hard disk support
- **Process Management**: Planning user mode support and multitasking capabilities
- **Network Stack**: Exploring TCP/IP implementation for network connectivity
- **Advanced Filesystems**: Considering support for modern filesystems like EXT4

## Explore the Code

The complete source code is available in this repository, organized into logical modules:
- `src/` - Core kernel implementation
- `include/` - Header files and API definitions  
- `boot/` - Assembly language boot code
- `documentation_of_inner_workings.md` - Detailed technical documentation

Each file includes extensive comments explaining the implementation decisions and technical challenges solved.

---

NanOS represents my deep dive into systems programming and computer science fundamentals. It demonstrates not just technical skills, but the persistence and problem-solving ability required to build complex systems from scratch. Whether you're interested in operating systems, low-level programming, or just curious about how computers work, I hope this project provides insight into the fascinating world of systems software. 