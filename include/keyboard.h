#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include "driver.h"

typedef void (*keyboard_event_callback_t)(char c);

void keyboard_set_callback(keyboard_event_callback_t cb);
void keyboard_irq_handler(void);
void keyboard_init(void);
void keyboard_shutdown(void);
void keyboard_driver_register(void);

// Keyboard buffer API
int keyboard_buffer_empty(void);
char keyboard_getchar(void);

#endif // KEYBOARD_H 