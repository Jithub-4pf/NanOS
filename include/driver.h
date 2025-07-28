#ifndef DRIVER_H
#define DRIVER_H

typedef void (*driver_init_fn)(void);
typedef void (*driver_shutdown_fn)(void);
typedef void (*driver_irq_handler_fn)(void);

typedef struct driver {
    const char* name;
    driver_init_fn init;
    driver_shutdown_fn shutdown;
    driver_irq_handler_fn irq_handler; // Optional, can be NULL
} driver_t;

void register_driver(driver_t* drv);

#endif // DRIVER_H 