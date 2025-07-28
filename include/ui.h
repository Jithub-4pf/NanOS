#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>

// UI API
void ui_initialize(void);
void ui_draw_status_bar(const char* status, uint8_t color);
void ui_draw_panel(int x, int y, int w, int h, const char* title, const char* content, uint8_t color);
void ui_clear_panel(int x, int y, int w, int h);
void ui_draw_prompt(const char* prompt, const char* input, uint8_t color);
void ui_update_interrupt_panel(int irq, const char* desc);
void ui_update_keystroke_panel(const char* buffer);
void ui_toggle(bool enabled);
extern bool ui_modern_enabled;

#endif // UI_H 