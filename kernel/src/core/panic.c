#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/kprintf.h>
#include <core/panic.h>

#define KERNEL_HALF 0xffff800000000000ULL   /* kernel addresses live above this */
#define MAX_FRAMES  16

/* Walk the saved frame pointers.
 * Works because the build keeps frame pointers (-fno-omit-frame-pointer).
 * Each frame is [saved rbp][return address]. */
static void backtrace(uint64_t rbp) {
    kprintf("backtrace:\n");
    uint64_t *fp = (uint64_t *)rbp;
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (((uintptr_t)fp & 7) || (uint64_t)(uintptr_t)fp < KERNEL_HALF)
            break;
        uint64_t ret = fp[1];
        if (ret == 0)
            break;
        kprintf("  #%-2d %p\n", i, (void *)ret);
        uint64_t *next = (uint64_t *)fp[0];
        if (next <= fp)         /* the stack grows down, so frames must go up */
            break;
        fp = next;
    }
}

void panic_at(const char *file, int line, const char *fmt, ...) {
    cpu_cli();

    /* A panic inside the panic handler would loop forever; just stop. */
    static volatile bool panicking;
    if (panicking)
        cpu_halt_forever();
    panicking = true;

    uint64_t rsp, rbp, rflags, cr2, cr3;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    __asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    kprintf("\n\x1b[1;91m*** SYMBIOTE PANIC ***\x1b[0m\n");

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\nat %s:%d\n", file, line);

    const char *ext = ext_current();
    if (ext)
        kprintf("while initialising extension: %s\n", ext);

    kprintf("rsp=%p rbp=%p rflags=%llx\n", (void *)rsp, (void *)rbp,
            (unsigned long long)rflags);
    kprintf("cr2=%p cr3=%p\n", (void *)cr2, (void *)cr3);
    backtrace(rbp);
    kprintf("resolve addresses:  addr2line -e kernel/bin-x86_64/symbiote <addr>\n");
    kprintf("\nSystem halted.\n");

    cpu_halt_forever();
}
