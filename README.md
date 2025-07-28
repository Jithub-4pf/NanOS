# NanOS - A Minimal x86 Operating System

NanOS is a simple 32-bit x86 operating system kernel written in C and x86 assembly. It boots via GRUB using the Multiboot standard and provides basic kernel facilities including memory management, interrupt handling, and a simple shell interface.

## Features

- **Multiboot-compliant bootloader** - Boots via GRUB
- **Memory Management** - Paging system with dynamic page allocation
- **Interrupt Handling** - GDT/IDT setup with exception and IRQ handling
- **Keyboard Input** - US QWERTY keyboard driver with circular buffer
- **ext2 Filesystem Support** - Read/write ext2 filesystem with VFS layer
- **Shell Interface** - Command-line interface with filesystem commands
- **Heap Management** - Dynamic memory allocation with free-list allocator
- **Block Device Layer** - Abstract interface for storage devices
- **RAM Disk** - In-memory block device for testing

## Shell Commands

- `ls [dir]` - List directory contents
- `cat <file>` - Display file contents
- `stat <file|dir>` - Show detailed file/directory info
- `touch <file>` - Create a new file
- `rm <file>` - Remove a file
- `mkdir <dir>` - Create a directory
- `rmdir <dir>` - Remove an empty directory
- `ln -s <target> <link>` - Create a symbolic link
- `chmod <mode> <file>` - Change file permissions
- `chown <uid:gid> <file>` - Change file ownership
- `echo <msg> [> file]` - Print or write text
- `pwd` - Print working directory
- `whoami` - Print current user
- `date` - Print system uptime
- `hexdump <file>` - Hex dump a file
- `clear` - Clear the screen
- `meminfo` - Show heap usage
- `fstest` - Test block device and filesystem
- `ps` - Show running processes
- `uptime` - Show system uptime
- `version` - Show OS version
- `help` - List all commands
- `reboot` - Restart the system

## Building

### Prerequisites

- i686-elf-gcc cross-compiler
- i686-elf-as assembler
- i686-elf-ld linker
- grub-mkrescue (for ISO creation)
- QEMU (for testing)

### Build Commands

```bash
# Build the kernel and create ISO
make all

# Clean build artifacts
make clean

# Run in QEMU
make run
```

## Project Structure

```
NanOS/
├── boot/           # Boot assembly code
├── src/            # Kernel source files
├── include/        # Header files
├── drivers/        # Device drivers
├── fonts/          # Font files
├── linker.ld       # Linker script
├── Makefile        # Build configuration
├── grub.cfg        # GRUB configuration
└── README.md       # This file
```

## Architecture

- **32-bit x86 protected mode**
- **Flat memory model** with paging
- **Modular driver system** for device management
- **Virtual filesystem (VFS)** layer for filesystem abstraction
- **ext2 filesystem** support with read/write capabilities
- **Heap-based memory management** with dynamic allocation

## Development Status

This is a learning project for operating system development. The kernel provides a solid foundation for further development including:

- User mode and system calls
- Multitasking and process management
- Advanced memory management
- Network stack
- Additional device drivers

## License

This project is for educational purposes. See individual source files for licensing information.

## Contributing

This is primarily a learning project, but contributions and improvements are welcome. Please ensure any changes maintain the educational focus and code quality. 