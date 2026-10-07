#ifndef CORE_SHELL_H
#define CORE_SHELL_H

/* Interactive command loop.
 * Reads a line from the keyboard, splits it into words,
 * and hands the words to the command registry (core/cmd.h),
 * which finds the command, checks the arguments against the parameters it declared, and runs it.
 * The shell itself knows no commands: they live in extensions (src/ext/cmd_*.c).
 *
 * Never returns. Call it from kmain after ext_init_all(). */
__attribute__((noreturn))
void shell_run(void);

#endif
