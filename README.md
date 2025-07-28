# NanOS - A Complete Operating System Built From Scratch

**NanOS** is a fully functional 32-bit x86 operating system that I built entirely from the ground up using C and assembly language. What started as curiosity about how computers really work became a comprehensive OS featuring industry-standard components like a complete EXT2 filesystem, virtual memory management, and a professional shell interface.

## Why I Built This

I've always been fascinated by the lowest levels of computing - the bare metal where software meets hardware. While most developers work with high-level frameworks and APIs, I wanted to understand what happens underneath it all. How does a computer boot? How do filesystems actually store data? How does memory management work at the hardware level?

Building NanOS let me explore these fundamental questions by implementing every component myself, from the bootloader to the filesystem driver. It's been an incredible journey into systems programming, hardware interfaces, and the elegant complexity that makes modern computing possible.

## What Makes NanOS Special

**Complete Filesystem Implementation**: NanOS includes a full read/write EXT2 filesystem - the same format used by Linux systems. This means it can create, modify, and organize files just like a real operating system, complete with directories, permissions, and metadata.

**Professional Memory Management**: The system features virtual memory with paging (4KB pages), dynamic memory allocation, and automatic page fault handling. This provides the same memory protection and flexibility found in production operating systems.

**Hardware Integration**: NanOS communicates directly with computer hardware - managing interrupts, handling keyboard input, controlling the display, and coordinating between the CPU and peripherals without any existing OS layer.

**Shell Interface**: A complete command-line interface with 22+ commands including `ls`, `cat`, `mkdir`, `chmod`, and file redirection. You can navigate directories, create and edit files, and manage the filesystem just like on Unix/Linux systems.

**Bootloader Integration**: The system boots using GRUB (the same bootloader used by Linux), following industry-standard Multiboot protocols to initialize hardware and load the kernel.

## Technical Architecture

- **32-bit x86 protected mode** with full privilege separation
- **Virtual memory system** with 4KB paging and demand allocation  
- **Complete EXT2 filesystem** with read/write support, symbolic links, and Unix permissions
- **Virtual filesystem (VFS) layer** for clean filesystem abstraction
- **Hardware drivers** for keyboard, display, and interrupt controllers
- **Heap-based memory management** with automatic allocation and deallocation
- **Modular kernel design** with clear separation between components

## Getting Started

### Installing QEMU

**On Debian/Ubuntu:**
```bash
sudo apt update
sudo apt install qemu-system-x86 build-essential grub-pc-bin grub-common xorriso
```

**On macOS:**
```bash
# Using Homebrew
brew install qemu

# Using MacPorts
sudo port install qemu
```

### Quick Start
```bash
# Clone the repository
git clone https://github.com/Jithub-4pf/NanOS.git
cd NanOS

# Run the pre-built kernel (no compilation needed!)
qemu-system-i386 -kernel myos.bin -m 32M
```

That's it! The repository includes a pre-built `myos.bin` kernel image that runs immediately in QEMU.

### Exploring the Code
The codebase is organized for clarity:
- `src/` - Core kernel implementation  
- `include/` - System headers and interfaces
- `boot/` - Assembly bootloader code
- `documentation_of_inner_workings.md` - Detailed technical documentation

Each component is well-documented and designed to be educational. Start with `src/kernel.c` to see the initialization sequence, then explore the filesystem code in `src/ext2.c` or memory management in `src/paging.c`.

## What's Next

The next major milestone is **persistent storage** - replacing the current RAM-based filesystem with an IDE/ATA driver for real hard drives. This will enable true data persistence across reboots and make NanOS suitable for practical use.

Future development includes:
- **User mode and system calls** for running user programs safely
- **Process management and multitasking** for running multiple programs  
- **Network stack** for internet connectivity
- **Additional hardware drivers** for broader device support

## A Personal Achievement

Building an operating system from scratch has been one of the most challenging and rewarding projects I've undertaken. Every line of code represents deep problem-solving, from debugging obscure hardware interactions to designing clean software architectures. It's given me profound appreciation for the complexity hidden beneath everyday computing and confidence that I can tackle any systems-level challenge.

NanOS proves that with persistence and curiosity, it's possible to understand and build even the most fundamental pieces of computer science. It's not just a technical project - it's a testament to the power of diving deep and building something meaningful from first principles.

---

*Built with passion for systems programming and a drive to understand how computers really work.*