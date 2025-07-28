#include "monitor.h"
#include "io.h"
#include <stdarg.h>

enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) 
{
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) 
{
	return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t monitor_row;
size_t monitor_column;
uint8_t monitor_color;
uint16_t* monitor_buffer;

void monitor_initialize(void) 
{
	monitor_row = 0;
	monitor_column = 0;
	monitor_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	monitor_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			monitor_buffer[index] = vga_entry(' ', monitor_color);
		}
	}
}

void monitor_clear(void) 
{
	monitor_row = 0;
	monitor_column = 0;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			monitor_buffer[index] = vga_entry(' ', monitor_color);
		}
	}
}

void monitor_setcolor(uint8_t color) 
{
	monitor_color = color;
}

void monitor_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
	const size_t index = y * VGA_WIDTH + x;
	monitor_buffer[index] = vga_entry(c, color);
}

void monitor_scroll(void) {
    // Move each line up by one
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            monitor_buffer[(y - 1) * VGA_WIDTH + x] = monitor_buffer[y * VGA_WIDTH + x];
        }
    }
    // Clear the last line
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        monitor_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', monitor_color);
    }
}

void monitor_putchar(char c) 
{
	if (c != '\n') {
        monitor_putentryat(c, monitor_color, monitor_column, monitor_row);
        if (++monitor_column == VGA_WIDTH) {
            monitor_column = 0;
            if (++monitor_row == VGA_HEIGHT) {
                monitor_scroll();
                monitor_row = VGA_HEIGHT - 1;
            }
        }
    } else {
        monitor_column = 0;
        if (++monitor_row == VGA_HEIGHT) {
            monitor_scroll();
            monitor_row = VGA_HEIGHT - 1;
        }
    }
}

void monitor_write_Sized(const char* data, size_t size) 
{
	for (size_t i = 0; i < size; i++)
		monitor_putchar(data[i]);
}

void monitor_write(const char* data) 
{
	monitor_write_Sized(data, strlen(data));
}


// Write a decimal number to the monitor
void monitor_write_dec(uint32_t n) {
    char buffer[11]; // Enough for 32-bit integer
    int i = 0;

    if (n == 0) {
        monitor_putchar('0');
        return;
    }

    while (n > 0) {
        buffer[i++] = '0' + (n % 10);
        n /= 10;
    }

    // Reverse buffer
    for (int j = i - 1; j >= 0; j--) {
        monitor_putchar(buffer[j]);
    }
}

// Write a newline to the monitor
void monitor_write_newline(void) {
    monitor_putchar('\n');
}

// Write a hexadecimal number to the terminal
void monitor_write_hex(uint32_t n) {
    static const char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];
    int i = 0;

    if (n == 0) {
        monitor_putchar('0');
        return;
    }

    while (n > 0) {
        buffer[i++] = hex_chars[n & 0xF];
        n >>= 4;
    }

    // Reverse buffer
    for (int j = i - 1; j >= 0; j--) {
        monitor_putchar(buffer[j]);
    }
}

// Simple snprintf implementation
int snprintf(char* buffer, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int written = 0;

    // Basic and limited implementation
    // Handles %s, %d, and %x (hexadecimal)
    while (*format && written < size - 1) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* str = va_arg(args, const char*);
                while (*str && written < size - 1) {
                    buffer[written++] = *str++;
                }
            } else if (*format == 'd') {
                int num = va_arg(args, int);
                char num_buffer[12];
                int i = 0;

                if (num == 0) {
                    num_buffer[i++] = '0';
                } else {
                    int is_negative = num < 0;
                    if (is_negative) {
                        num = -num;
                    }
                    while (num > 0) {
                        num_buffer[i++] = '0' + (num % 10);
                        num /= 10;
                    }
                    if (is_negative) {
                        num_buffer[i++] = '-';
                    }
                    // Reverse num_buffer
                    for (int j = i - 1; j >= 0; j--) {
                        buffer[written++] = num_buffer[j];
                    }
                }
            } else if (*format == 'x') {
                unsigned int num = va_arg(args, unsigned int);
                char num_buffer[9];
                int i = 0;

                if (num == 0) {
                    num_buffer[i++] = '0';
                } else {
                    while (num > 0) {
                        num_buffer[i++] = "0123456789ABCDEF"[num & 0xF];
                        num >>= 4;
                    }
                    // Reverse num_buffer
                    for (int j = i - 1; j >= 0; j--) {
                        buffer[written++] = num_buffer[j];
                    }
                }
            }
        } else {
            buffer[written++] = *format;
        }
        format++;
    }
    buffer[written] = '\0';
    va_end(args);

    return written;
}

void monitor_get_cursor(int* row, int* col) {
    if (row) *row = monitor_row;
    if (col) *col = monitor_column;
}

void monitor_set_cursor(int row, int col) {
    if (row >= 0 && row < (int)VGA_HEIGHT) monitor_row = row;
    if (col >= 0 && col < (int)VGA_WIDTH) monitor_column = col;
    // Update hardware VGA cursor
    uint16_t pos = monitor_row * VGA_WIDTH + monitor_column;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
