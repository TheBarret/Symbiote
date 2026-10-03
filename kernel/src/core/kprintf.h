#ifndef CORE_KPRINTF_H
#define CORE_KPRINTF_H

#include <stdarg.h>
#include <stddef.h>

/* Formatted output to every console.
 * Supports: %c %s %d %i %u %x %X %p %%  with flags '-' and '0', a width
 * (number or '*'), and the length modifiers l, ll, z (all 64-bit). */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);

/* Same formatting into a buffer. Always NUL-terminates (if cap > 0) and
 * returns the length the full output WOULD have had. */
int ksnprintf(char *buf, size_t cap, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
int kvsnprintf(char *buf, size_t cap, const char *fmt, va_list ap);

#endif
