#include "ui.h"
#include "monitor.h"
#include <stdbool.h>
#include "heap.h"

bool ui_modern_enabled = true;

static char last_status[80] = "";
static char last_interrupt[32] = "None";
static char last_keystrokes[64] = "";

// Minimal string functions for freestanding kernel
static size_t kstrlen(const char* s) { size_t l = 0; while (s && s[l]) ++l; return l; }
static void kstrncpy(char* dst, const char* src, size_t n) { size_t i = 0; for (; i < n-1 && src && src[i]; ++i) dst[i] = src[i]; if (n) dst[i] = 0; }

// Helper to format numbers as K/M
void format_size(int value, char* out, int out_size) {
    if (value >= 1024*1024) {
        snprintf(out, out_size, "%dM", value/(1024*1024));
    } else if (value >= 1024) {
        snprintf(out, out_size, "%dK", value/1024);
    } else {
        snprintf(out, out_size, "%d", value);
    }
}

void ui_initialize(void) {
    // Optionally clear screen or draw base UI
    ui_draw_all();
}

void ui_draw_status_bar(const char* status, uint8_t color) {
    monitor_setcolor(color);
    monitor_set_cursor(0, 0);
    for (int i = 0; i < 80; ++i) {
        if (status[i] && status[i] != '\0')
            monitor_putchar(status[i]);
        else
            monitor_putchar(' ');
    }
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
    kstrncpy(last_status, status, sizeof(last_status));
}

void ui_draw_panel(int x, int y, int w, int h, const char* title, const char* content, uint8_t color) {
    monitor_setcolor(color);
    for (int i = 0; i < h; ++i) {
        monitor_set_cursor(y + i, x);
        for (int j = 0; j < w; ++j) {
            if (i == 0 && j < (int)kstrlen(title))
                monitor_putchar(title[j]);
            else if (i == 1 && j < (int)kstrlen(content))
                monitor_putchar(content[j]);
            else
                monitor_putchar(' ');
        }
    }
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
}

void ui_clear_panel(int x, int y, int w, int h) {
    monitor_setcolor(MONITOR_COLOR_BLACK);
    for (int i = 0; i < h; ++i) {
        monitor_set_cursor(y + i, x);
        for (int j = 0; j < w; ++j) {
            monitor_putchar(' ');
        }
    }
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
}

void ui_draw_prompt(const char* prompt, const char* input, uint8_t color) {
    monitor_setcolor(color);
    monitor_set_cursor(23, 0);
    for (int i = 0; i < 80; ++i) monitor_putchar(' ');
    monitor_set_cursor(23, 0);
    monitor_write(prompt);
    monitor_write(input);
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
}

void ui_update_interrupt_panel(int irq, const char* desc) {
    snprintf(last_interrupt, sizeof(last_interrupt), "IRQ: %d %s", irq, desc ? desc : "");
    ui_draw_panel(60, 1, 20, 2, "Interrupt", last_interrupt, MONITOR_COLOR_CYAN);
}

void ui_update_keystroke_panel(const char* buffer) {
    kstrncpy(last_keystrokes, buffer, sizeof(last_keystrokes));
    ui_draw_panel(60, 4, 20, 2, "Keystrokes", last_keystrokes, MONITOR_COLOR_GREEN);
}

void ui_draw_heap_panel(void) {
    size_t total, used, free;
    heap_stats(&total, &used, &free);
    char heap_labels[20] = "Total  Used  Free";
    char t[8], u[8], f[8];
    format_size((int)total, t, sizeof(t));
    format_size((int)used, u, sizeof(u));
    format_size((int)free, f, sizeof(f));
    char heap_values[32];
    snprintf(heap_values, sizeof(heap_values), "%s  %s  %s", t, u, f);
    ui_draw_panel(60, 18, 20, 2, "Heap", heap_values, MONITOR_COLOR_MAGENTA);
    // Print values on the second line of the panel
    monitor_setcolor(MONITOR_COLOR_MAGENTA);
    monitor_set_cursor(60, 19);
    monitor_write(heap_values);
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
}

void ui_draw_separator(int y) {
    monitor_setcolor(MONITOR_COLOR_DARK_GREY);
    monitor_set_cursor(y, 0);
    for (int i = 0; i < 80; ++i) monitor_putchar('-');
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
}

void ui_draw_vertical_separator(int x, int y_start, int y_end) {
    monitor_setcolor(MONITOR_COLOR_DARK_GREY);
    for (int y = y_start; y <= y_end; ++y) {
        monitor_set_cursor(y, x);
        monitor_putchar('|');
    }
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);
}

void ui_draw_all(void) {
    // Status bar
    ui_draw_status_bar(" NanOS Modern UI | F12: Toggle | ESC: Help | Ctrl+L: Clear ", MONITOR_COLOR_BLUE);
    ui_draw_separator(1);

    // Vertical separator between main and right panels
    ui_draw_vertical_separator(59, 2, 21);

    // Clear right panel area (x=60, y=2..21, w=20, h=20)
    ui_clear_panel(60, 2, 20, 20);

    // Interrupt panel (top right)
    ui_draw_panel(60, 2, 20, 3, "Interrupt", last_interrupt, MONITOR_COLOR_CYAN);
    // Keystrokes panel (middle right)
    ui_draw_panel(60, 6, 20, 3, "Keystrokes", last_keystrokes, MONITOR_COLOR_GREEN);
    // Heap panel (bottom right)
    size_t total, used, free;
    heap_stats(&total, &used, &free);
    char heap_labels[20] = "Total  Used  Free";
    char t[8], u[8], f[8];
    format_size((int)total, t, sizeof(t));
    format_size((int)used, u, sizeof(u));
    format_size((int)free, f, sizeof(f));
    char heap_values[32];
    snprintf(heap_values, sizeof(heap_values), "%s  %s  %s", t, u, f);
    ui_draw_panel(60, 18, 20, 2, "Heap", heap_labels, MONITOR_COLOR_MAGENTA);
    // Print values on the second line of the panel
    monitor_setcolor(MONITOR_COLOR_MAGENTA);
    monitor_set_cursor(60, 19);
    monitor_write(heap_values);
    monitor_setcolor(MONITOR_COLOR_LIGHT_GREY);

    // Separator above prompt
    ui_draw_separator(22);
    // Prompt
    ui_draw_prompt("NanOS> ", "", MONITOR_COLOR_LIGHT_GREY);
}

void ui_toggle(bool enabled) {
    ui_modern_enabled = enabled;
} 