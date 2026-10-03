#ifndef CORE_CONSOLE_H
#define CORE_CONSOLE_H

#include <stddef.h>

/* Plugin console module */
struct console_ops {
    const char *name;
    void (*write)(const char *buf, size_t len);
};

/* Attach a console */
void console_register(const struct console_ops *ops);

/* Send text to every attached console (and into the boot log). */
void console_write(const char *buf, size_t len);

#endif
