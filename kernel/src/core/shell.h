#ifndef CORE_SHELL_H
#define CORE_SHELL_H

/* Interactive command loop.
 * Reads lines from the keyboard, dispatches the first word against a
 * fixed table of built-in commands, and echoes the result to every
 * registered console.
 *
 * Never returns. Call it from kmain after ext_init_all(). */
__attribute__((noreturn))
void shell_run(void);

#endif
