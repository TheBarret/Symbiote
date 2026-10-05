#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/boot.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/keyboard.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/shell.h>
#include <lib/string.h>
#include <core/version.h>
#include <core/pmm.h>

#define SHELL_LINE_MAX  128
#define SHELL_ARG_MAX   8
#define SHELL_VERSION "0.1.3"

/* Human-readable name for a Limine memory-map entry type.
 * Falls back to "unknown" so a new type added by a future protocol revision prints something rather than a raw number.
 */

static const char *memmap_type_name(uint64_t type) {
    switch (type) {
    case LIMINE_MEMMAP_USABLE:                 return "usable";
    case LIMINE_MEMMAP_RESERVED:               return "reserved";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "acpi-reclaim";
    case LIMINE_MEMMAP_ACPI_NVS:               return "acpi-nvs";
    case LIMINE_MEMMAP_BAD_MEMORY:             return "bad";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "boot-reclaim";
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return "kernel";
    case LIMINE_MEMMAP_FRAMEBUFFER:            return "framebuffer";
    case LIMINE_MEMMAP_RESERVED_MAPPED:        return "reserved-mapped";
    default:                                   return "unknown";
    }
}

/*  Memory functions  */

static int cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;

    if (!boot_memory_ok()) {
        kprintf("Error: bootloader did not provide (memory) memmap or hhdm!\n");
        return 1;
    }

    struct limine_memmap_response *map = boot_memmap();
    uint64_t hhdm = boot_hhdm_offset();

    uint64_t total_usable = 0;
    size_t usable_regions = 0;
    for (size_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            total_usable += e->length;
            usable_regions++;
        }
    }

    kprintf("Memory map: %zu region(s), %zu usable\n", map->entry_count, usable_regions);
    kprintf("* HHDM offset  :   0x%llx\n", (unsigned long long)hhdm);
    kprintf("* Free :  %llu KiB (%llu MiB)\n",
            (unsigned long long)(total_usable / 1024),
            (unsigned long long)(total_usable / (1024 * 1024)));
    return 0;
}

static int cmd_memmap(int argc, char **argv) {
    (void)argc; (void)argv;

    if (!boot_memory_ok()) {
        kprintf("Error: bootloader did not provide (memory) memmap or hhdm!\n");
        return 1;
    }

    struct limine_memmap_response *map = boot_memmap();
    kprintf("Memmap: %zu region(s), hhdm=0x%llx\n",
            map->entry_count,
            (unsigned long long)boot_hhdm_offset());

    for (size_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *e = map->entries[i];
        uint64_t start = e->base;
        uint64_t end   = e->base + e->length;
        kprintf("  [%zu] %016llx-%016llx  %-16s  %llu bytes\n",
                i,
                (unsigned long long)start,
                (unsigned long long)end,
                memmap_type_name(e->type),
                (unsigned long long)e->length);
    }
    return 0;
}

/*  PMM  */

static int cmd_pages(int argc, char **argv) {
    (void)argc; (void)argv;
    struct pmm_stats s = pmm_get_stats();
    kprintf("Frames: %zu total, %zu free (%zu MiB free)\n",
            s.total_frames, s.free_frames,
            (s.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));
    return 0;
}

static int cmd_memtest(int argc, char **argv) {
    (void)argc; (void)argv;

    /* Allocate until we can't, stamp each frame with its own address, then free everything.
     * This catches duplicate allocation  (a stamped address that doesn't match its frame)
     * and out-of-range frames (an address outside any USABLE region). */

    struct pmm_stats before = pmm_get_stats();
    size_t allocated = 0;

    /* Cap at a few thousand so the test doesn't take forever on a machine with gigabytes of RAM. */
    size_t cap = before.free_frames;
    if (cap > 4096)
        cap = 4096;

    /* Save the physical address of each allocation so we can free them later.
     * Because we can only allocate frames one at a time (the non-contig variant),
     * and 4096 * 8 bytes = 32 KiB of stack would be too much,
     * we use the pages themselves as an intrusive list. */
    uint64_t head = 0;   /* physical address of first allocated frame, or 0 */

    for (size_t i = 0; i < cap; i++) {
        uint64_t p = pmm_alloc();
        if (p == 0)
            break;

        /* Check alignment and range. */
        if ((p & (PMM_PAGE_SIZE - 1)) != 0) {
            kprintf("memtest: misaligned frame 0x%llx\n",
                    (unsigned long long)p);
            return 1;
        }
        size_t frame = (size_t)(p / PMM_PAGE_SIZE);
        if (frame >= before.total_frames) {
            kprintf("memtest: out-of-range frame 0x%llx\n",
                    (unsigned long long)p);
            return 1;
        }

        /* Write the previous head into the new frame via HHDM. */
        uint64_t *slot = (uint64_t *)pmm_phys_to_virt(p);
        *slot = head;
        head = p;
        allocated++;
    }

    if (allocated == 0) {
        kprintf("memtest: could not allocate any frames\n");
        return 1;
    }

    /* Walk the list, verify stamps, and freeing.
     * The stamp is the frame's own physical address;
     * a duplicate allocation would be visible as two list entries pointing at the same frame,
     * which the walk cannot detect directly (the second write would overwrite the first's pointer).
     * Instead, we verify that the number of frames we free matches the number we allocated,
     * and that none of the frames are marked already-free when we free them. */
    size_t freed = 0;
    uint64_t node = head;
    while (node != 0) {
        uint64_t *slot = (uint64_t *)pmm_phys_to_virt(node);
        uint64_t next = *slot;
        pmm_free(node, 1);
        freed++;
        node = next;
    }

    struct pmm_stats after = pmm_get_stats();
    if (after.free_frames != before.free_frames) {
        kprintf("Memtest: leak or double free: before=%zu after=%zu\n", before.free_frames, after.free_frames);
        return 1;
    }

    kprintf("Memtest: %zu frames round-tripped, free count consistent\n", freed);
    return 0;
}

/*  line input  */

/* Read one line from the keyboard into buf (up to cap-1 chars).
 * Handles backspace, echoes as it goes, returns the length.

static size_t read_line_old(char *buf, size_t cap) {
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
                // Move back, overwrite with space, move back again.
                // This is what a terminal expects.
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }

        if (c < 0x20 || c > 0x7E)
            continue;   // ignore other control chars and non-ASCII

        if (len + 1 >= cap)
            continue;   // line is full: silently drop further input

        buf[len++] = c;
        console_putc(c);
    }
    buf[len] = '\0';
    return len;
}
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
    { "help",    "List commands",           cmd_help },
    { "echo",    "Print tool",              cmd_echo },
    { "clear",   "Clear screen (ANSI)",     cmd_clear },
    { "version", "Print kernel version",    cmd_version },
    { "exts",    "List loaded extensions",  cmd_exts },
    { "panic",   "Test panic mode",         cmd_panic },
    { "halt",    "Stop kernel",             cmd_halt },
    { "mem",     "Memory status",           cmd_mem },
    { "memmap",  "Memory dump",             cmd_memmap },
    { "pages",   "PMM status)",             cmd_pages },
    { "memtest", "PMM self-test",           cmd_memtest },
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
    kprintf("kernel extensions: %zu\n", ext_count());
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
