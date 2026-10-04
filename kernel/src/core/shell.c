#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/keyboard.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/shell.h>
#include <lib/string.h>
#include <core/version.h>

#define SHELL_LINE_MAX  128
#define SHELL_ARG_MAX   8
#define SHELL_VERSION "0.1.0"

/*  line input  */

/* Read one line from the keyboard into buf (up to cap-1 chars).
 * Handles backspace, echoes as it goes, returns the length. */
static size_t read_line(char *buf, size_t cap) {
    size_t len = 0;
    for (;;) {
        char c = kbd_getchar();

        if (c == '\n') {
            console_putc('\n');
            break;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                /* Move back, overwrite with space, move back again.
                 * This is what a terminal expects. */
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }

        if (c < 0x20 || c > 0x7E)
            continue;   /* ignore other control chars and non-ASCII */

        if (len + 1 >= cap)
            continue;   /* line is full: silently drop further input */

        buf[len++] = c;
        console_putc(c);
    }
    buf[len] = '\0';
    return len;
}

/*  tokenizer  */

/* Split line in place into argv. Returns argc (capped at SHELL_ARG_MAX).
 * Modifies the buffer: replaces spaces with NULs. */
static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t')
            *p++ = '\0';
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
    }
    return argc;
}

/*  built-in commands  */

static int cmd_help(int argc, char **argv);
static int cmd_echo(int argc, char **argv);
static int cmd_clear(int argc, char **argv);
static int cmd_version(int argc, char **argv);
static int cmd_exts(int argc, char **argv);
static int cmd_panic(int argc, char **argv);
static int cmd_halt(int argc, char **argv);

struct command {
    const char *name;
    const char *help;
    int (*fn)(int argc, char **argv);
};

static const struct command commands[] = {
    { "help",    "List commands",          cmd_help },
    { "echo",    "Print tool",             cmd_echo },
    { "clear",   "Clear screen (ANSI)",    cmd_clear },
    { "version", "Print kernel version",   cmd_version },
    { "exts",    "List loaded extensions", cmd_exts },
    { "panic",   "Test panic mode",        cmd_panic },
    { "halt",    "Stop kernel",            cmd_halt },
};
#define NCOMMANDS (sizeof commands / sizeof commands[0])

static int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("commands:\n");
    for (size_t i = 0; i < NCOMMANDS; i++)
        kprintf("  %-8s  %s\n", commands[i].name, commands[i].help);
    return 0;
}

static int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) console_putc(' ');
        kprintf("%s", argv[i]);
    }
    console_putc('\n');
    return 0;
}

static int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    /* ANSI: ESC[2J clears the screen, ESC[H homes the cursor. */
    kprintf("\x1b[2J\x1b[H");
    return 0;
}

static int cmd_version(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Shell %s, Kernel: %s\n", SHELL_VERSION, SYM_VERSION);
    return 0;
}

static int cmd_exts(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("%zu extension(s) attached.\n", ext_count());
    return 0;
}

static int cmd_panic(int argc, char **argv) {
    (void)argc; (void)argv;
    PANIC("Panic Test");
    return 1;
}

static int cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("Stopping...\n");
    cpu_halt_forever();
    return 0;
}

/*  dispatcher  */

static void run_command(char *line) {
    char *argv[SHELL_ARG_MAX];
    int argc = tokenize(line, argv, SHELL_ARG_MAX);
    if (argc == 0)
        return;

    for (size_t i = 0; i < NCOMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            int rc = commands[i].fn(argc, argv);
            if (rc != 0)
                kprintf("%s: exit %d\n", argv[0], rc);
            return;
        }
    }
    kprintf("%s: command not found. Try 'help'.\n", argv[0]);
}

/*  the loop  */

void shell_run(void) {
    if (!kbd_init()) {
        kprintf("Error: no PS/2 keyboard detected!\n");
        cpu_halt_forever();
    }

    kprintf("\nType 'help' for a list of commands.\n");

    char line[SHELL_LINE_MAX];
    for (;;) {
        kprintf("> ");
        size_t len = read_line(line, sizeof line);
        if (len == 0)
            continue;
        run_command(line);
    }
}
