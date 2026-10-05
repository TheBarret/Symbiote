#ifndef CORE_KLOG_H
#define CORE_KLOG_H

/* Primitive kernel boot logger.
 * kprintf remains the raw entry point for output that should not carry the tag:
 * the shell prompt, command output, the panic dump, and the extension system's own per-extension lines. */

/* The tag as it appears at the start of every klog() line.
 * Six visible characters, followed by a reset so the message body prints in the terminal's default color.
 * Width is fixed at 6 so the message column aligns across every line. */
//#define SYM_TAG "\x1b[1;92m[KERN]\x1b[0m"
#define SYM_TAG "\x1b[1;91m[KERN]\x1b[0m"

/* Same idea for the extension subsystem's own lines. Cyan so it is visually distinct from the kernel tag.
 * Trailing space pads it to the same 6-character width. */
//#define SYM_TAG_EXT "\x1b[1;96m[EXT ]\x1b[0m"
#define SYM_TAG_EXT "\x1b[1;96m[EXT ]\x1b[0m"

/* Print a tagged line to every console, like kprintf with SYM_TAG prepended. Format arguments are checked by the compiler. */
void klog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Same as klog but with the extension tag.
 * Used by ext.c and by extensions that want their output visually grouped with the extension subsystem's own lines. */
void klog_ext(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
