#ifndef MONITOR_H
#define MONITOR_H

#include <stdint.h>
#include <stddef.h>

// Function prototypes for monitor output
void monitor_initialize(void);
void monitor_setcolor(uint8_t color);
void monitor_putchar(char c);
void monitor_write(const char* str);
void monitor_write_hex(uint32_t n);
void monitor_write_dec(uint32_t n);
void monitor_write_newline(void);
void monitor_clear(void);
size_t strlen(const char* str) ;
// Function prototype for snprintf
int snprintf(char* buffer, size_t size, const char* format, ...);
// Get the current cursor position (row, col)
void monitor_get_cursor(int* row, int* col);
// Set the cursor position (row, col)
void monitor_set_cursor(int row, int col);
void monitor_scroll(void);

#define MONITOR_COLOR_BLACK 0x0
#define MONITOR_COLOR_BLUE 0x1
#define MONITOR_COLOR_GREEN 0x2
#define MONITOR_COLOR_CYAN 0x3
#define MONITOR_COLOR_RED 0x4
#define MONITOR_COLOR_MAGENTA 0x5
#define MONITOR_COLOR_BROWN 0x6
#define MONITOR_COLOR_LIGHT_GREY 0x7
#define MONITOR_COLOR_DARK_GREY 0x8
#define MONITOR_COLOR_LIGHT_BLUE 0x9
#define MONITOR_COLOR_LIGHT_GREEN 0xA
#define MONITOR_COLOR_LIGHT_CYAN 0xB
#define MONITOR_COLOR_LIGHT_RED 0xC
#define MONITOR_COLOR_LIGHT_MAGENTA 0xD
#define MONITOR_COLOR_LIGHT_BROWN 0xE
#define MONITOR_COLOR_WHITE 0xF

#endif // MONITOR_H
