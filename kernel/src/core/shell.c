#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/string.h>
#include <core/cmd.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/keyboard.h>
#include <core/kprintf.h>
#include <core/shell.h>
#include <core/cmd.h>

#define SHELL_LINE_MAX  128
#define SHELL_ARG_MAX   (CMD_MAX_ARGS + 1)      /* the command name + its arguments */

/* Built-in help */
static int cmd_help_fn(int argc, char **argv) {
    if (argc == 1) {
        /* List every registered command. */
        size_t n = cmd_count();
        kprintf("commands (%zu):\n", n);
        for (size_t i = 0; i < n; i++) {
            const struct sym_cmd *c = cmd_at(i);
            kprintf("  %-12s  %s\n", c->name, c->help);
        }
        return 0;
    }

    if (argc == 2) {
        /* Detail for one command. */
        const struct sym_cmd *c = cmd_find(argv[1]);
        if (!c) {
            kprintf("help: no such command: %s\n", argv[1]);
            return 1;
        }
        kprintf("%s: %s\n", c->name, c->help);
        cmd_print_usage(c);
        return 0;
    }

    kprintf("usage: help [command]\n");
    return 2;
}

/*  line input  */

/* Read one line from the keyboard into buf (up to cap-1 chars).
 * Handles backspace, echoes as it goes, returns the length.
 */
static size_t read_line(char *buf, size_t cap) {
    size_t len = 0;
    for (;;) {
        char c = kbd_getchar();

        if (c == '\n' || c == '\r') {
            console_putc('\n');
            break;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }

        if (c < 0x20 || c > 0x7E)
            continue;

        if (len + 1 >= cap)
            continue;

        buf[len++] = c;
        console_putc(c);
    }
    buf[len] = '\0';
    return len;
}

/*  tokenizer  */

/* Split line in place into argv. Returns argc, or -1 if the line has more
 * than `max` words (the old version silently dropped the extras).
 * Modifies the buffer: replaces spaces with NULs. */
static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    for (;;) {
        while (*p == ' ' || *p == '\t')
            *p++ = '\0';
        if (!*p)
            break;
        if (argc == max)
            return -1;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
    }
    return argc;
}

/*  one line  */

static void run_line(char *line) {
    char *argv[SHELL_ARG_MAX];
    int argc = tokenize(line, argv, SHELL_ARG_MAX);
    if (argc < 0) {
        kprintf("Too many arguments (max=%d)\n",
                CMD_MAX_ARGS);
        return;
    }
    if (argc == 0)
        return;

    if (strcmp(argv[0], "help") == 0) {
        int rc = cmd_help_fn(argc, argv);
        if (rc == 2)
            kprintf("Usage: help [command]\n");
        return;
    }

    /* The registry does everything else: look up, validate, run, report. */
    cmd_dispatch(argc, argv);
}

/*  the loop  */

void shell_run(void) {
    if (!kbd_init()) {
        kprintf("Error: no PS/2 keyboard detected!\n");
        cpu_halt_forever();
    }

    /* Catch duplicate names and broken parameter specs now, loudly, instead
     * of when somebody happens to type the command. Silent when all is well. */
    int problems = cmd_selfcheck();
    if (problems)
        kprintf("Warning: %d issues detected in look-up table.\n", problems);

    kprintf("\nType 'help' for a list of commands.\n");

    char line[SHELL_LINE_MAX];
    for (;;) {
        kprintf("> ");
        size_t len = read_line(line, sizeof line);
        if (len == 0)
            continue;
        run_line(line);
    }
}
