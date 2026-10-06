#include <core/cmd.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/cmd.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/version.h>

/* System and diagnostics  */

/*  version  */

static int cmd_version_fn(const struct cmd_args *a) {
    (void)a;
    kprintf("Symbiote %s\n", SYM_VERSION);
    return CMD_OK;
}

SYM_COMMAND(version, "", "Print kernel version", cmd_version_fn);

/*  exts  */

static int cmd_exts_fn(const struct cmd_args *a) {
    (void)a;
    kprintf("extensions (shared: %zu, system: %zu)...ok\n", ext_count(), cmd_count());
    return CMD_OK;
}

SYM_COMMAND(exts, "", "Display extension counts", cmd_exts_fn);

/*  clear  */

static int cmd_clear_fn(const struct cmd_args *a) {
    (void)a;
    /* ANSI: ESC[2J clears the screen, ESC[H homes the cursor. */
    kprintf("\x1b[2J\x1b[H");
    return CMD_OK;
}

SYM_COMMAND(clear, "", "Clear screen (ANSI)", cmd_clear_fn);

/*  panic  */

static int cmd_panic_fn(const struct cmd_args *a) {
    /* Optional message. The spec is [message:str...], so the words are
     * joined back into a single line before printing. */
    char buf[128];
    size_t n = 0;
    for (int i = 0; i < a->argc && n + 1 < sizeof buf; i++) {
        if (i > 0 && n + 1 < sizeof buf)
            buf[n++] = ' ';
        const char *p = a->argv[i];
        while (*p && n + 1 < sizeof buf)
            buf[n++] = *p++;
    }
    buf[n] = '\0';

    if (n == 0)
        PANIC("Test Panic!");
    PANIC("%s", buf);
    return CMD_OK;      /* not reached; PANIC is noreturn */
}

SYM_COMMAND(panic, "[message:str...]", "Relinquish and panic", cmd_panic_fn);

/*  halt  */

static int cmd_halt_fn(const struct cmd_args *a) {
    (void)a;
    kprintf("halting.\n");
    cpu_halt_forever();
    return CMD_OK;      /* not reached */
}

SYM_COMMAND(halt, "", "Stop Machine", cmd_halt_fn);
