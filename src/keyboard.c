#include "keyboard.h"
#include "driver.h"
#include <stdint.h>
#include <stddef.h>
#include "monitor.h"
#include "io.h"
#include "heap.h"
#include "process.h" // Added for process_t

#define KBD_BUF_SIZE 128
static char* kbd_buffer = NULL;
static int kbd_buf_size = 0;
static int kbd_buf_head = 0;
static int kbd_buf_tail = 0;

static keyboard_event_callback_t key_callback = 0;

// US QWERTY scancode set 1
static const char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, // Control
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, // Alt, Space
    // F1-F12, etc. (not handled)
};
static const char scancode_map_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, // Control
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0,
};

static int shift = 0;

extern process_t* shell_proc; // Added for process_t

void keyboard_set_callback(keyboard_event_callback_t cb) {
    key_callback = cb;
}

void keyboard_buffer_init(int size) {
    if (kbd_buffer) kfree(kbd_buffer);
    kbd_buffer = (char*)kmalloc(size);
    kbd_buf_size = size;
    kbd_buf_head = 0;
    kbd_buf_tail = 0;
}

int keyboard_buffer_empty(void) {
    return kbd_buf_head == kbd_buf_tail;
}

char keyboard_getchar(void) {
    if (kbd_buf_head == kbd_buf_tail) return 0;
    char c = kbd_buffer[kbd_buf_tail];
    kbd_buf_tail = (kbd_buf_tail + 1) % kbd_buf_size;
    return c;
}

void keyboard_irq_handler(void) {
    uint8_t scancode = inb(0x60);
    char c = 0;
    // Handle shift press/release
    if (scancode == 0x2A || scancode == 0x36) { shift = 1; return; } // Shift down
    if (scancode == 0xAA || scancode == 0xB6) { shift = 0; return; } // Shift up
    if (scancode & 0x80) return; // Key release, ignore
    if (shift)
        c = scancode_map_shift[scancode];
    else
        c = scancode_map[scancode];
    // Handle backspace
    if (c == '\b') {
        if (kbd_buf_head != kbd_buf_tail) {
            // Remove last char from buffer
            kbd_buf_head = (kbd_buf_head - 1 + kbd_buf_size) % kbd_buf_size;
        }
        if (key_callback) key_callback(c);
        return;
    }
    // Handle enter
    if (c == '\n') {
        int next_head = (kbd_buf_head + 1) % kbd_buf_size;
        if (next_head != kbd_buf_tail) {
            kbd_buffer[kbd_buf_head] = c;
            kbd_buf_head = next_head;
        }
        if (key_callback) key_callback(c);
        return;
    }
    // Only buffer printable characters
    if (c >= 32 && c <= 126) {
        int next_head = (kbd_buf_head + 1) % kbd_buf_size;
        if (next_head != kbd_buf_tail) { // Not full
            kbd_buffer[kbd_buf_head] = c;
            kbd_buf_head = next_head;
        }
        if (key_callback) key_callback(c);
    }
    // At the end of the handler, wake up shell if blocked
    if (shell_proc && shell_proc->state == TASK_BLOCKED) {
        shell_proc->state = TASK_READY;
    }
}

void keyboard_init(void) {
    // Nothing needed for basic PS/2 keyboard
}

void keyboard_shutdown(void) {
    if (kbd_buffer) kfree(kbd_buffer);
    kbd_buffer = NULL;
    kbd_buf_size = 0;
}

// Register the driver
void keyboard_driver_register(void) {
    static driver_t keyboard_driver = {
        .name = "keyboard",
        .init = keyboard_init,
        .shutdown = keyboard_shutdown,
        .irq_handler = keyboard_irq_handler
    };
    keyboard_buffer_init(128); // Default size
    register_driver(&keyboard_driver);
} 