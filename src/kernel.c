#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "idt.h"
#include "monitor.h"
#include "gdt.h"
#include "pic.h"
#include "io.h"
#include "keyboard.h"
#include "ui.h"
#include "heap.h"
#include "paging.h"
#include "physmem.h"
#include "multiboot.h"
#include "sched.h"
#include "process.h"
#include "blockdev.h"
#include "ramdisk.h"
#include "ext2.h"
#include "vfs.h"
#include "ext2_image.h"
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This OS will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This OS needs to be compiled with a ix86-elf compiler"
#endif

#define SHELL_BUF_SIZE 128
#define MAX_ARGS 8

char shell_input[SHELL_BUF_SIZE];
int shell_input_len = 0;

extern process_t* shell_proc;

// String utility functions
static int strcmp_local(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

static int strncmp_local(const char* a, const char* b, size_t n) {
    while (n > 0 && *a && *b && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : *a - *b;
}

static void strcpy_local(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static size_t strlen_local(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

// Simple shell tokenizer
static int shell_tokenize(char* input, char* argv[], int max_args) {
    int argc = 0;
    while (*input && argc < max_args) {
        // Skip leading spaces
        while (*input == ' ' || *input == '\t') input++;
        if (!*input) break;
        argv[argc++] = input;
        // Find end of token
        while (*input && *input != ' ' && *input != '\t') input++;
        if (*input) {
            *input = 0;
            input++;
        }
    }
    return argc;
}

// Command table for usage and argument count
struct shell_cmd_info {
    const char* name;
    int min_args;
    int max_args;
    const char* usage;
};
static const struct shell_cmd_info shell_cmds[] = {
    {"help",    1, 1, "help"},
    {"ls",      1, 2, "ls [dir]"},
    {"cat",     2, 2, "cat <file>"},
    {"stat",    2, 2, "stat <file|dir>"},
    {"clear",   1, 1, "clear"},
    {"meminfo", 1, 1, "meminfo"},
    {"fstest",  1, 1, "fstest"},
    {"ps",      1, 1, "ps"},
    {"uptime",  1, 1, "uptime"},
    {"version", 1, 1, "version"},
    {"echo",    2, MAX_ARGS, "echo <msg> [> file]"},
    {"touch",   2, 2, "touch <file>"},
    {"rm",      2, 2, "rm <file>"},
    {"mkdir",   2, 2, "mkdir <dir>"},
    {"rmdir",   2, 2, "rmdir <dir>"},
    {"pwd",     1, 1, "pwd"},
    {"whoami",  1, 1, "whoami"},
    {"date",    1, 1, "date"},
    {"hexdump", 2, 2, "hexdump <file>"},
    {"ln",      4, 4, "ln -s <target> <link>"},
    {"chmod",   3, 3, "chmod <mode> <file>"},
    {"chown",   3, 3, "chown <uid:gid> <file>"},
    {"reboot",  1, 1, "reboot"},
    {NULL, 0, 0, NULL}
};
#define NUM_SHELL_CMDS (sizeof(shell_cmds)/sizeof(shell_cmds[0]))

static const struct shell_cmd_info* shell_find_cmd(const char* name) {
    for (size_t i = 0; i < NUM_SHELL_CMDS; ++i) {
        if (strcmp_local(name, shell_cmds[i].name) == 0) return &shell_cmds[i];
    }
    return NULL;
}

// Command processing functions
static void process_command(char* cmdline) {
    char* argv[MAX_ARGS];
    int argc = shell_tokenize(cmdline, argv, MAX_ARGS);
    if (argc == 0) return;
    const char* cmd = argv[0];
    const struct shell_cmd_info* info = shell_find_cmd(cmd);
    if (!info) {
        monitor_write("Unknown command: "); monitor_write(cmd); monitor_write("\nType 'help' for available commands.\n");
        return;
    }
    if (argc < info->min_args || argc > info->max_args) {
        monitor_write("Usage: "); monitor_write(info->usage); monitor_write("\n");
        return;
    }
    if (strcmp_local(cmd, "help") == 0) {
        monitor_write("Available commands:\n");
        monitor_write("  help                - Show this help message\n");
        monitor_write("  ls [dir]            - List directory contents\n");
        monitor_write("  cat <file>          - Display file contents\n");
        monitor_write("  stat <file|dir>     - Show file/directory information\n");
        monitor_write("  clear               - Clear the screen\n");
        monitor_write("  meminfo             - Show memory information\n");
        monitor_write("  fstest              - Test filesystem infrastructure\n");
        monitor_write("  ps                  - Show running processes\n");
        monitor_write("  uptime              - Show system uptime\n");
        monitor_write("  version             - Show OS version information\n");
        monitor_write("  echo <msg>          - Echo text back\n");
        monitor_write("  echo <msg> > <file> - Write text to file\n");
        monitor_write("  touch <file>        - Create a new file\n");
        monitor_write("  rm <file>           - Delete a file\n");
        monitor_write("  mkdir <dir>         - Create a directory\n");
        monitor_write("  rmdir <dir>         - Remove a directory\n");
        monitor_write("  pwd                 - Print working directory\n");
        monitor_write("  whoami              - Print current user\n");
        monitor_write("  date                - Print system date/time\n");
        monitor_write("  hexdump <file>      - Hex dump a file\n");
        monitor_write("  ln -s <target> <link> - Create symbolic link\n");
        monitor_write("  chmod <mode> <file> - Change file permissions\n");
        monitor_write("  chown <uid:gid> <file> - Change file ownership\n");
        monitor_write("  reboot              - Restart the system\n");
        return;
    }
    if (strcmp_local(cmd, "ls") == 0) {
        const char* dir = (argc > 1) ? argv[1] : "/";
        if (ext2_get_fs() == NULL) {
            monitor_write("No filesystem mounted. Use 'fstest' to test infrastructure.\n");
        } else if (vfs_list_directory(dir) != 0) {
            monitor_write("Error: Could not list directory\n");
        }
            return;
        }
    if (strcmp_local(cmd, "cat") == 0 && argc > 1) {
        const char* filename = argv[1];
        char filepath[256]; filepath[0] = '/'; strcpy_local(filepath + 1, filename);
        vfs_file_t* file = vfs_open(filepath);
        if (!file) { monitor_write("Error: Could not open file '"); monitor_write(filename); monitor_write("'\n"); return; }
        char buffer[1024];
        int bytes_read = vfs_read(file, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) { buffer[bytes_read] = '\0'; monitor_write(buffer); monitor_write("\n"); }
        else { monitor_write("Error: Could not read file\n"); }
        vfs_close(file);
            return;
        }
    if (strcmp_local(cmd, "stat") == 0 && argc > 1) {
        const char* filename = argv[1];
        char filepath[256]; filepath[0] = '/'; strcpy_local(filepath + 1, filename);
        vfs_dirent_t stat;
        if (vfs_stat(filepath, &stat) == 0) {
            monitor_write("File: "); monitor_write(stat.name); monitor_write("\n");
            monitor_write("Type: ");
            if (stat.type == VFS_TYPE_DIR) monitor_write("Directory\n");
            else if (stat.type == VFS_TYPE_SYMLINK) {
                monitor_write("Symbolic link\n");
                // Show symlink target
                ext2_fs_t* fs = ext2_get_fs();
                if (fs) {
                    ext2_inode_t inode;
                    if (ext2_read_inode(fs, stat.inode, &inode) == 0) {
                        char target[256];
                        if (ext2_read_symlink(fs, &inode, target, sizeof(target)) == 0) {
                            monitor_write("Target: "); monitor_write(target); monitor_write("\n");
                        }
                    }
                }
            }
            else monitor_write("Regular file\n");
            monitor_write("Size: "); monitor_write_dec(stat.size); monitor_write(" bytes\n");
            monitor_write("Inode: "); monitor_write_dec(stat.inode); monitor_write("\n");
            
            // Show permissions and ownership
            ext2_fs_t* fs = ext2_get_fs();
            if (fs) {
                ext2_inode_t inode;
                if (ext2_read_inode(fs, stat.inode, &inode) == 0) {
                    // Convert mode to octal and string
                    monitor_write("Mode: ");
                    monitor_write("0");
                    monitor_write_dec((inode.i_mode >> 9) & 7); // User
                    monitor_write_dec((inode.i_mode >> 6) & 7); // Group
                    monitor_write_dec((inode.i_mode >> 3) & 7); // Other
                    monitor_write(" (");
                    char perms[11];
                    ext2_mode_to_string(fs, inode.i_mode, perms);
                    monitor_write(perms);
                    monitor_write(")\n");
                    
                    monitor_write("Uid: "); monitor_write_dec(inode.i_uid); 
                    monitor_write("  Gid: "); monitor_write_dec(inode.i_gid); monitor_write("\n");
                    
                    monitor_write("Links: "); monitor_write_dec(inode.i_links_count); monitor_write("\n");
                    
                    // Show timestamps
                    char time_buf[32];
                    monitor_write("Access: ");
                    ext2_format_time(inode.i_atime, time_buf);
                    monitor_write(time_buf);
                    monitor_write("\n");
                    
                    monitor_write("Modify: ");
                    ext2_format_time(inode.i_mtime, time_buf);
                    monitor_write(time_buf);
                    monitor_write("\n");
                    
                    monitor_write("Change: ");
                    ext2_format_time(inode.i_ctime, time_buf);
                    monitor_write(time_buf);
            monitor_write("\n");
                }
            }
        } else { monitor_write("Error: Could not stat file '"); monitor_write(filename); monitor_write("'\n"); }
        return;
    }
    if (strcmp_local(cmd, "clear") == 0) {
        monitor_clear();
        return;
    }
    if (strcmp_local(cmd, "meminfo") == 0) {
        size_t total, used, free;
        heap_stats(&total, &used, &free);
        monitor_write("Memory Information:\n");
        monitor_write("  Total heap: "); monitor_write_dec(total / 1024); monitor_write(" KiB\n");
        monitor_write("  Used heap:  "); monitor_write_dec(used / 1024); monitor_write(" KiB\n");
        monitor_write("  Free heap:  "); monitor_write_dec(free / 1024); monitor_write(" KiB\n");
        return;
    }
    if (strcmp_local(cmd, "fstest") == 0) {
        monitor_write("Filesystem Infrastructure Test:\n");
        
        // Test block device
        blockdev_t* ramdisk = blockdev_get("ramdisk0");
        if (ramdisk) {
            monitor_write("  Block device 'ramdisk0': OK\n");
            monitor_write("  Block count: ");
            monitor_write_dec(blockdev_get_block_count(ramdisk));
            monitor_write("\n");
            monitor_write("  Block size:  ");
            monitor_write_dec(blockdev_get_block_size(ramdisk));
            monitor_write(" bytes\n");
            
            // Test basic read/write
            char test_data[512] = "Hello, ext2 filesystem test!";
            char read_data[512];
            if (blockdev_write(ramdisk, 0, 1, test_data) == 0) {
                monitor_write("  Block write: OK\n");
                if (blockdev_read(ramdisk, 0, 1, read_data) == 0) {
                    monitor_write("  Block read:  OK\n");
                    monitor_write("  Data integrity: ");
                    if (strcmp_local(test_data, read_data) == 0) {
                        monitor_write("OK\n");
                    } else {
                        monitor_write("FAIL\n");
                    }
                } else {
                    monitor_write("  Block read:  FAIL\n");
                }
            } else {
                monitor_write("  Block write: FAIL\n");
            }
        } else {
            monitor_write("  Block device 'ramdisk0': NOT FOUND\n");
        }
        
        // Test filesystem
        if (ext2_get_fs() == NULL) {
            monitor_write("  ext2 filesystem: NOT MOUNTED\n");
            monitor_write("  Note: This is expected without an ext2 image\n");
        } else {
            monitor_write("  ext2 filesystem: MOUNTED\n");
        }
        
        monitor_write("Infrastructure test complete.\n");
        return;
    }
    if (strcmp_local(cmd, "ps") == 0) {
        monitor_write("Running Processes:\n");
        monitor_write("PID  STATE    NAME\n");
        monitor_write("---  -------  --------\n");
        
        extern process_t* process_list;
        extern process_t* current;
        if (process_list) {
            process_t* p = process_list;
            int count = 0;
            do {
                monitor_write_dec(p->pid);
                if (p->pid < 10) monitor_write("   ");
                else if (p->pid < 100) monitor_write("  ");
                else monitor_write(" ");
                
                switch (p->state) {
                    case TASK_RUNNING:
                        monitor_write("RUNNING ");
                        break;
                    case TASK_READY:
                        monitor_write("READY   ");
                        break;
                    case TASK_BLOCKED:
                        monitor_write("BLOCKED ");
                        break;
                    case TASK_TERMINATED:
                        monitor_write("TERM    ");
                        break;
                    default:
                        monitor_write("UNKNOWN ");
                        break;
                }
                
                if (p == current) {
                    monitor_write(" [CURRENT]");
                } else if (p->pid == 1) {
                    monitor_write(" idle");
                } else if (p->pid == 2) {
                    monitor_write(" shell");
                } else {
                    monitor_write(" process");
                    monitor_write_dec(p->pid);
                }
                monitor_write("\n");
                
                p = p->next;
                count++;
                if (count > 10) break; // Safety check
            } while (p != process_list);
        }
        return;
    }
    if (strcmp_local(cmd, "uptime") == 0) {
        extern uint32_t system_ticks;
        uint32_t seconds = system_ticks / 100; // Assuming 100 Hz timer
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        
        monitor_write("System uptime: ");
        if (hours > 0) {
            monitor_write_dec(hours);
            monitor_write(" hours, ");
        }
        monitor_write_dec(minutes % 60);
        monitor_write(" minutes, ");
        monitor_write_dec(seconds % 60);
        monitor_write(" seconds (");
        monitor_write_dec(system_ticks);
        monitor_write(" ticks)\n");
        return;
    }
    if (strcmp_local(cmd, "version") == 0) {
        monitor_write("NanOS v1.0 - ext2 Filesystem Edition\n");
        monitor_write("Built with i686-elf-gcc for x86-32\n");
        monitor_write("Features: VGA, GDT, IDT, Paging, Heap, Multitasking, ext2, VFS\n");
        monitor_write("Copyright (c) 2024 NanOS Project\n");
        return;
    }
    if (strcmp_local(cmd, "touch") == 0 && argc > 1) {
        const char* filename = argv[1];
        char filepath[256]; filepath[0] = '/'; strcpy_local(filepath + 1, filename);
        if (vfs_exists(filepath)) { monitor_write("File already exists.\n"); return; }
        if (vfs_create(filepath, VFS_TYPE_FILE) == 0) monitor_write("File created.\n");
        else monitor_write("Error: Could not create file.\n");
        return;
    }
    if (strcmp_local(cmd, "rm") == 0 && argc > 1) {
        const char* filename = argv[1];
        char filepath[256]; filepath[0] = '/'; strcpy_local(filepath + 1, filename);
        if (!vfs_exists(filepath)) { monitor_write("File does not exist.\n"); return; }
        if (vfs_unlink(filepath) == 0) monitor_write("File deleted.\n");
        else monitor_write("Error: Could not delete file.\n");
        return;
    }
    if (strcmp_local(cmd, "mkdir") == 0 && argc > 1) {
        const char* dirname = argv[1];
        char dirpath[256]; dirpath[0] = '/'; strcpy_local(dirpath + 1, dirname);
        if (vfs_exists(dirpath)) { monitor_write("Directory already exists.\n"); return; }
        if (vfs_create(dirpath, VFS_TYPE_DIR) == 0) monitor_write("Directory created.\n");
        else monitor_write("Error: Could not create directory.\n");
        return;
    }
    if (strcmp_local(cmd, "rmdir") == 0 && argc > 1) {
        const char* dirname = argv[1];
        char dirpath[256]; dirpath[0] = '/'; strcpy_local(dirpath + 1, dirname);
        vfs_dirent_t stat;
        if (vfs_stat(dirpath, &stat) != 0) { monitor_write("Error: Directory not found.\n"); return; }
        if (stat.type != VFS_TYPE_DIR) { monitor_write("Error: Not a directory.\n"); return; }
        
        // Check if directory is empty
        ext2_fs_t* fs = ext2_get_fs();
        if (fs) {
            ext2_inode_t inode;
            if (ext2_read_inode(fs, stat.inode, &inode) == 0) {
                int is_empty = ext2_is_dir_empty(fs, &inode);
                if (is_empty == 0) {
                    monitor_write("Error: Directory not empty.\n");
                    return;
                }
            }
        }
        
        if (vfs_unlink(dirpath) == 0) monitor_write("Directory removed.\n");
        else monitor_write("Error: Could not remove directory.\n");
        return;
    }
    if (strcmp_local(cmd, "pwd") == 0) {
        monitor_write("/\n"); // Only root for now
        return;
    }
    if (strcmp_local(cmd, "whoami") == 0) {
        monitor_write("root\n");
        return;
    }
    if (strcmp_local(cmd, "date") == 0) {
        extern uint32_t system_ticks;
        uint32_t seconds = system_ticks / 100;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        monitor_write("Uptime: ");
        if (hours > 0) { monitor_write_dec(hours); monitor_write("h "); }
        monitor_write_dec(minutes % 60); monitor_write("m ");
        monitor_write_dec(seconds % 60); monitor_write("s\n");
        return;
    }
    if (strcmp_local(cmd, "hexdump") == 0 && argc > 1) {
        const char* filename = argv[1];
        char filepath[256]; filepath[0] = '/'; strcpy_local(filepath + 1, filename);
        vfs_file_t* file = vfs_open(filepath);
        if (!file) { monitor_write("Error: Could not open file.\n"); return; }
        char buffer[16];
        uint32_t offset = 0;
        int bytes;
        while ((bytes = vfs_read(file, buffer, 16)) > 0) {
            monitor_write("  "); monitor_write_dec(offset); monitor_write(": ");
            for (int i = 0; i < bytes; ++i) {
                char hex[3] = {0};
                int v = (unsigned char)buffer[i];
                hex[0] = "0123456789ABCDEF"[v >> 4];
                hex[1] = "0123456789ABCDEF"[v & 0xF];
                monitor_write(hex); monitor_write(" ");
            }
            for (int i = bytes; i < 16; ++i) monitor_write("   ");
            monitor_write(" |");
            for (int i = 0; i < bytes; ++i) {
                char c = buffer[i];
                monitor_putchar((c >= 32 && c <= 126) ? c : '.');
            }
            monitor_write("|\n");
            offset += bytes;
        }
        vfs_close(file);
        return;
    }
    if (strcmp_local(cmd, "ln") == 0 && argc == 4) {
        if (strcmp_local(argv[1], "-s") != 0) {
            monitor_write("Error: Only symbolic links are supported (use -s)\n");
            return;
        }
        const char* target = argv[2];
        const char* linkname = argv[3];
        char linkpath[256];
        linkpath[0] = '/';
        strcpy_local(linkpath + 1, linkname);
        
        if (vfs_create_symlink(linkpath, target) == 0) {
            monitor_write("Symbolic link created.\n");
        } else {
            monitor_write("Error: Could not create symbolic link.\n");
        }
        return;
    }
    if (strcmp_local(cmd, "chmod") == 0 && argc == 3) {
        const char* mode_str = argv[1];
        const char* filename = argv[2];
        char filepath[256];
        filepath[0] = '/';
        strcpy_local(filepath + 1, filename);
        
        // Parse octal mode
        uint16_t mode = 0;
        for (int i = 0; mode_str[i]; i++) {
            if (mode_str[i] < '0' || mode_str[i] > '7') {
                monitor_write("Error: Invalid mode (use octal, e.g., 755)\n");
                return;
            }
            mode = (mode << 3) | (mode_str[i] - '0');
        }
        
        if (vfs_chmod(filepath, mode) == 0) {
            monitor_write("Permissions changed.\n");
        } else {
            monitor_write("Error: Could not change permissions.\n");
        }
        return;
    }
    if (strcmp_local(cmd, "chown") == 0 && argc == 3) {
        const char* owner_str = argv[1];
        const char* filename = argv[2];
        char filepath[256];
        filepath[0] = '/';
        strcpy_local(filepath + 1, filename);
        
        // Parse uid:gid
        uint16_t uid = 0, gid = 0;
        int colon_pos = -1;
        for (int i = 0; owner_str[i]; i++) {
            if (owner_str[i] == ':') {
                colon_pos = i;
                break;
            }
        }
        
        if (colon_pos < 0) {
            monitor_write("Error: Invalid format (use uid:gid)\n");
            return;
        }
        
        // Parse uid
        for (int i = 0; i < colon_pos; i++) {
            if (owner_str[i] < '0' || owner_str[i] > '9') {
                monitor_write("Error: Invalid uid\n");
                return;
            }
            uid = uid * 10 + (owner_str[i] - '0');
        }
        
        // Parse gid
        for (int i = colon_pos + 1; owner_str[i]; i++) {
            if (owner_str[i] < '0' || owner_str[i] > '9') {
                monitor_write("Error: Invalid gid\n");
                return;
            }
            gid = gid * 10 + (owner_str[i] - '0');
        }
        
        if (vfs_chown(filepath, uid, gid) == 0) {
            monitor_write("Ownership changed.\n");
        } else {
            monitor_write("Error: Could not change ownership.\n");
        }
        return;
    }
    if (strcmp_local(cmd, "echo") == 0 && argc > 1) {
        // Support echo ... > file
        int gt = -1;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] == '>' && argv[i][1] == 0) { gt = i; break; }
        }
        if (gt > 0 && gt < argc - 1) {
            // echo ... > file
            char filepath[256]; filepath[0] = '/'; strcpy_local(filepath + 1, argv[gt+1]);
            vfs_file_t* f = vfs_open(filepath);
            if (!f) {
                if (vfs_create(filepath, VFS_TYPE_FILE) != 0) {
                    monitor_write("Error: Could not create file.\n");
                    return;
                }
                f = vfs_open(filepath);
            }
            if (!f) { monitor_write("Error: Could not open file.\n"); return; }
            vfs_truncate(f, 0); // overwrite
            // Join argv[1]..argv[gt-1] as message
            char msg[256]; int pos = 0;
            for (int i = 1; i < gt && pos < 255; ++i) {
                int l = strlen_local(argv[i]);
                if (pos + l + 1 >= 255) break;
                strcpy_local(msg + pos, argv[i]);
                pos += l;
                msg[pos++] = ' ';
            }
            if (pos > 0) msg[pos-1] = 0; else msg[0] = 0;
            int written = vfs_write(f, msg, strlen_local(msg));
            vfs_close(f);
            if (written == (int)strlen_local(msg)) monitor_write("Wrote to file.\n");
            else monitor_write("Error: Write failed.\n");
        } else {
            // Just echo
            for (int i = 1; i < argc; ++i) {
                monitor_write(argv[i]);
                if (i < argc - 1) monitor_write(" ");
            }
        monitor_write("\n");
        }
        return;
    }
    if (strcmp_local(cmd, "reboot") == 0) {
        monitor_write("Rebooting system...\n");
        for (volatile int i = 0; i < 10000000; i++);
        __asm__ volatile("cli; hlt; int $0x03");
        return;
    }
    monitor_write("Unknown command: "); monitor_write(cmd); monitor_write("\nType 'help' for available commands.\n");
}

void shell_process(void) {
    monitor_setcolor(0x0B); // Cyan prompt
    monitor_write("\nNanOS Shell with ext2 filesystem support\n");
    if (ext2_get_fs() != NULL) {
        monitor_write("ext2 filesystem is mounted and ready!\n");
        monitor_write("Try: ls, cat hello.txt, cat readme.txt\n");
    } else {
        monitor_write("Filesystem not available.\n");
    }
    monitor_write("Type 'help' for available commands.\n");
    monitor_write("\nNanOS> ");
    shell_input_len = 0;
    while (1) {
        if (keyboard_buffer_empty()) {
            // Just wait for keyboard input without scheduler blocking
            __asm__ volatile("hlt");
            continue;
        }
        char c = keyboard_getchar();
        if (c == '\b') {
            if (shell_input_len > 0) {
                shell_input_len--;
                shell_input[shell_input_len] = '\0';
                int row, col;
                monitor_get_cursor(&row, &col);
                if (col > 0) {
                    monitor_set_cursor(row, col - 1);
                    monitor_putchar(' ');
                    monitor_set_cursor(row, col - 1);
                }
            }
        } else if (c == '\n') {
            shell_input[shell_input_len] = '\0';
            monitor_putchar('\n');
            
            // Process the command
            process_command(shell_input);
            
            monitor_setcolor(0x0B); // Cyan prompt
            monitor_write("NanOS> ");
            shell_input_len = 0;
        } else if (c >= 32 && c <= 126) {
            if (shell_input_len < SHELL_BUF_SIZE - 1) {
                shell_input[shell_input_len++] = c;
                shell_input[shell_input_len] = '\0';
                monitor_putchar(c);
            }
        }
        // scheduler_maybe_resched(); // Disabled for direct shell mode
    }
}

void idle_process(void) {
    while (1) {
        scheduler_maybe_resched();
        __asm__ volatile("hlt");
    }
}

void test_proc1(void) {
    int count = 0;
    char msg[32];
    while (1) {
        monitor_setcolor(0x0A); // Green
        monitor_write("[Task 1] Running\n");
        snprintf(msg, sizeof(msg), "Hello from 1, count %d", count);
        send_message(3, msg, strlen(msg) + 1); // Assume proc2 has pid 3
        for (volatile int i = 0; i < 1000000; ++i);
        count++;
        if (count % 3 == 0) {
            monitor_write("[Task 1] Sleeping for 5 ticks\n");
            process_sleep(5);
        }
        scheduler_maybe_resched();
    }
}

void test_proc2(void) {
    int count = 0;
    message_t m;
    while (1) {
        monitor_setcolor(0x0C); // Red
        monitor_write("[Task 2] Running\n");
        if (receive_message(&m) == 0) {
            monitor_write("[Task 2] Got message: ");
            monitor_write(m.data);
            monitor_write("\n");
        }
        for (volatile int i = 0; i < 1000000; ++i);
        count++;
        if (count == 5) {
            monitor_write("[Task 2] Exiting\n");
            process_exit();
        }
        scheduler_maybe_resched();
    }
}

void test_proc3(void) {
    while (1) {
        monitor_setcolor(0x0F); // Red
        monitor_write("[Task 2] Running\n");
        for (volatile int i = 0; i < 1000000; ++i);
        scheduler_maybe_resched();

    }
}

__attribute__((used)) __attribute__((cdecl)) void kernel_main(uint32_t multiboot_magic, multiboot_info_t* mb_info) {
    monitor_initialize();
    monitor_setcolor(0x1F); // Blue on white
    for (int i = 0; i < 5; ++i) monitor_write("\n");
    monitor_write("              =============================\n");
    monitor_write("                  Welcome to NanOS!        \n");
    monitor_write("              =============================\n");
    for (int i = 0; i < 3; ++i) monitor_write("\n");
    monitor_setcolor(0x07); // Reset to normal

    gdt_init();
    idt_init();
    pic_init();
    paging_init();
    paging_install_page_fault_handler();
    monitor_write("[BOOT] VGA/Monitor... [OK]\n");
    heap_init();
    monitor_write("[BOOT] Heap... [OK]\n");
    
    // Print heap statistics
    size_t total, used, free;
    heap_stats(&total, &used, &free);
    monitor_write("[BOOT] Heap: ");
    monitor_write_dec(total / 1024);
    monitor_write(" KiB total, ");
    monitor_write_dec(free / 1024);
    monitor_write(" KiB free\n");

    // Multiboot and physical memory
    monitor_write("[BOOT] Checking Multiboot magic... ");
    if (multiboot_magic != MULTIBOOT_MAGIC) {
        monitor_setcolor(0x4F); // Red on white
        monitor_write("[FAIL]\n[ERROR] Invalid Multiboot magic!\n");
        for (;;) { __asm__ volatile ("hlt"); }
    }
    monitor_write("[OK]\n");

    extern uintptr_t _start, _end;
    physmem_init(32 * 1024 * 1024, (uintptr_t)&_start, (uintptr_t)&_end);
    monitor_write("[BOOT] Physical Memory... [OK]\n");

    keyboard_driver_register();
    keyboard_buffer_init(128);  // Initialize keyboard buffer
    monitor_write("[BOOT] Keyboard... [OK]\n");

    // Initialize filesystem
    vfs_init();
    monitor_write("[BOOT] VFS... [OK]\n");
    
    // Create and mount a RAM disk for testing
    blockdev_t* ramdisk = ramdisk_create("ramdisk0", 256 * 1024); // 256KB for ext2
    if (ramdisk) {
        blockdev_register(ramdisk);
        monitor_write("[BOOT] RAM disk... [OK]\n");
        
        // Load ext2 filesystem image into RAM disk
        monitor_write("[BOOT] Loading ext2 filesystem image... ");
        if (ramdisk_load_ext2_image(ramdisk, test_fs_img, test_fs_img_len) == 0) {
            monitor_write("[OK]\n");
            
            // Mount the ext2 filesystem
            monitor_write("[BOOT] Mounting ext2 filesystem... ");
            if (vfs_mount_root() == 0) {
                monitor_write("[OK]\n");
                monitor_write("[BOOT] ext2 filesystem ready with test files!\n");
            } else {
                monitor_write("[FAILED]\n");
                monitor_write("[BOOT] Filesystem infrastructure ready but not mounted\n");
            }
        } else {
            monitor_write("[FAILED]\n");
            monitor_write("[BOOT] Could not load filesystem image\n");
        }
        
        // Print heap usage after filesystem setup
        size_t total, used, free;
        heap_stats(&total, &used, &free);
        monitor_write("[BOOT] Heap after filesystem: ");
        monitor_write_dec(used / 1024);
        monitor_write(" KiB used, ");
        monitor_write_dec(free / 1024);
        monitor_write(" KiB free\n");
    } else {
        monitor_write("[BOOT] RAM disk creation failed - out of memory\n");
    }

    monitor_write("[BOOT] Initializing processes... ");
    process_init();
    scheduler_init();
    monitor_write("[OK]\n");

    monitor_write("[BOOT] Creating processes... ");
    process_t* idle = process_create(idle_process, 4096);
    scheduler_add(idle);
    process_t* shell = process_create(shell_process, 4096);
    shell_proc = shell;
    scheduler_add(shell);
    monitor_write("[OK]\n");

    monitor_write("[BOOT] Starting timer... ");
    timer_init();
    monitor_write("[OK]\n");

    monitor_write("[BOOT] Enabling Interrupts... ");
    asm volatile ("sti");
    monitor_write("[OK]\n");

    monitor_write("[BOOT] Scheduler running.\n");

    // Enter idle loop; scheduler will switch tasks on timer interrupts
    idle_process();
}
