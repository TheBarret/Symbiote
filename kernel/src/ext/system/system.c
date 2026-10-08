#include <core/cmd.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/cmd.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/version.h>

/* System and diagnostics  */


#include <stddef.h>
#include <stdint.h>

#include <core/cmd.h>
#include <core/cpu.h>
#include <core/isr.h>
#include <core/keyboard.h>
#include <core/kprintf.h>
#include <core/timer.h>
#include <lib/string.h>

/* System-level commands & utilities */

/*  irqs  */
#define U(x) ((unsigned long long)(x))


/*  version  */

static int cmd_version_fn(const struct cmd_args *a) {
    (void)a;
    kprintf("Symbiote %s\n", SYM_VERSION);
    return CMD_OK;
}

SYM_COMMAND(version, "", "Display kernel version", cmd_version_fn);

/*  clear  */

static int cmd_clear_fn(const struct cmd_args *a) {
    (void)a;
    /* ANSI: ESC[2J clears the screen, ESC[H homes the cursor. */
    kprintf("\x1b[2J\x1b[H");
    return CMD_OK;
}

SYM_COMMAND(clear, "", "Clear screen (ANSI)", cmd_clear_fn);

/*  halt  */
static int cmd_halt_fn(const struct cmd_args *a) {
    (void)a;
    kprintf("halting.\n");
    cpu_halt_forever();
    return CMD_OK;      /* not reached */
}
SYM_COMMAND(halt, "", "Stop Machine", cmd_halt_fn);


/*  irqs  */

static int cmd_irqs(const struct cmd_args *a) {
    (void)a;
    kprintf("irq  handler   count        spurious  unhandled\n");
    for (int i = 0; i < 16; i++) {
        struct irq_info in = irq_get_info(i);
        if (!in.registered && in.count == 0 && in.spurious == 0 && in.unhandled == 0)
            continue;
        kprintf("%3d  %-8s  %-11llu  %-8llu  %llu\n", i, in.registered ? "yes" : "-",
                U(in.count), U(in.spurious), U(in.unhandled));
    }
    return 0;
}
SYM_COMMAND(irqs, "", "IRQ interrupt status", cmd_irqs);

/*  uptime */

static int cmd_uptime(const struct cmd_args *a) {
    (void)a;
    uint64_t ms = uptime_ms();
    kprintf("uptime %llu.%03llu seconds, %llu ms, %llu ticks at %u Hz\n",
            U(ms / 1000), U(ms % 1000), U(ms), U(timer_ticks()), timer_hz());
    return 0;
}
SYM_COMMAND(uptime, "", "Time since boot", cmd_uptime);

/*  sleep */

static int cmd_sleep(const struct cmd_args *a) {
    uint64_t remaining = a->num[0];
    uint64_t start = uptime_ms();

    /* Sleep in short slices so Esc can interrupt a long one. */
    while (remaining) {
        uint64_t slice = remaining > 20 ? 20 : remaining;
        timer_sleep_ms(slice);
        remaining -= slice;

        char c = 0;
        if (kbd_poll(&c) == KBD_SPECIAL && c == KBD_KEY_ESC) {
            kprintf("sleep: interrupted after %llu ms\n", U(uptime_ms() - start));
            return 1;
        }
    }
    kprintf("slept %llu ms\n", U(uptime_ms() - start));
    return 0;
}
SYM_COMMAND(sleep, "<ms:num>", "Sleep N milliseconds (Esc=interrupt)", cmd_sleep);

/* extension greeter */
static int system_ext_init(void) { return 0; }
SYM_EXTENSION(system_toolkit, system_ext_init, EXT_PRIO_APPLET);

/*  exts

static int cmd_exts_fn(const struct cmd_args *a) {
    (void)a;
    kprintf("extensions (shared: %zu, system: %zu)...ok\n", ext_count(), cmd_count());
    return CMD_OK;
}
SYM_COMMAND(exts, "", "Display extension counts", cmd_exts_fn);
*/

/*  panic

static int cmd_panic_fn(const struct cmd_args *a) {
    // Optional message. The spec is [message:str...], so the words are
    // joined back into a single line before printing.
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
    return CMD_OK;      // not reached; PANIC is noreturn
}

SYM_COMMAND(panic, "[message:str...]", "Test panic", cmd_panic_fn);
*/


/*  fault

static void __attribute__((noinline)) fault_divide(void) {
    __asm__ volatile ("xorl %%edx, %%edx\n\t"
                      "movl $1, %%eax\n\t"
                      "xorl %%ecx, %%ecx\n\t"
                      "divl %%ecx" ::: "rax", "rcx", "rdx");
}

static void __attribute__((noinline)) fault_ud(void) {
    __asm__ volatile ("ud2");
}
*/

/* Load a selector past the end of the GDT: #GP with a decodable error code.
static void __attribute__((noinline)) fault_gp(void) {
    __asm__ volatile ("movw $0x30, %%ax\n\t"
                      "movw %%ax, %%ds" ::: "rax");
}

static void __attribute__((noinline)) fault_pf(void) {
    // The empty asm hides the constant from the optimiser, which would
    // otherwise warn about (and might delete) a write to a fixed address.
    uint64_t addr = 0x10;
    __asm__ volatile ("" : "+r"(addr));
    *(volatile uint64_t *)addr = 0xDEAD;
}
*/


/* Point rsp at unmapped memory and push: the #PF cannot be delivered on that
 * stack, which escalates to a double fault. It only gets reported because
 * vector 8 runs on its own IST stack.
static void __attribute__((noinline)) fault_df(void) {
    __asm__ volatile ("movq $0, %%rsp\n\t"
                      "pushq %%rax" ::: "memory");
}

static void __attribute__((noinline)) fault_int3(void) {
    __asm__ volatile ("int3");
}

static const struct {
    const char *name;
    void (*fn)(void);
    const char *what;
} faults[] = {
    { "divide", fault_divide, "#DE  divide by zero" },
    { "ud",     fault_ud,     "#UD  invalid opcode" },
    { "gp",     fault_gp,     "#GP  bad segment selector" },
    { "pf",     fault_pf,     "#PF  write to an unmapped address" },
    { "df",     fault_df,     "#DF  double fault (bad stack, needs the IST)" },
    { "int3",   fault_int3,   "#BP  breakpoint" },
};
*/


/* fault tester
static int cmd_fault(const struct cmd_args *a) {
    for (size_t i = 0; i < sizeof faults / sizeof faults[0]; i++) {
        if (strcmp(a->argv[0], faults[i].name) == 0) {
            kprintf("triggering %s ...\n", faults[i].what);
            faults[i].fn();
            kprintf("fault: it came back, which should not happen\n");
            return 1;
        }
    }
    kprintf("fault: unknown kind '%s'. kinds:\n", a->argv[0]);
    for (size_t i = 0; i < sizeof faults / sizeof faults[0]; i++)
        kprintf("  %-7s %s\n", faults[i].name, faults[i].what);
    return CMD_USAGE;
}
SYM_COMMAND(fault, "<kind:str>", "Test faulting", cmd_fault);
*/
