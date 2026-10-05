#include <stdarg.h>
#include <stddef.h>
#include <core/klog.h>
#include <core/kprintf.h>

/* Both entry points are the same shape: a tag string,
 * then the caller's format and arguments.
 * The tag is emitted as a literal prefix so width modifiers in the caller's format never touch it. */

void klog(const char *fmt, ...) {
    kprintf(SYM_TAG " ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void klog_ext(const char *fmt, ...) {
    kprintf(SYM_TAG_EXT " ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
