#ifndef CORE_CPU_H
#define CORE_CPU_H

#include <stdint.h>

/* rdtsc field */

//static inline uint64_t rdtsc(void) {
//    uint32_t low, high;
//    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
//    return ((uint64_t)high << 32) | low;
//}

/* TSC field, Non-serializing; sufficient for an estimated timestamp. */
static inline uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

/* Discover TSC frequency in Hz.
 * Should be at CPUID 0x15, a family/model table for the ECX==0 case,
 * Returns 0 if no estimate is available. no panics, no blocks. */
uint64_t cpu_tsc_hz(void);

/* Port I/O */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Read `count` 16-bit words from a port into memory (ATA PIO uses this). */
static inline void insw(uint16_t port, void *addr, uint64_t count) {
    __asm__ volatile ("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint64_t count) {
    __asm__ volatile ("rep outsw" : "+S"(addr), "+c"(count) : "d"(port) : "memory");
}

/* cli/sti also act as compiler barriers ("memory"):
 * code that checks a flag an interrupt handler sets must not be reordered across them. */
static inline void cpu_cli(void) { __asm__ volatile ("cli" ::: "memory"); }
static inline void cpu_sti(void) { __asm__ volatile ("sti" ::: "memory"); }

static inline void cpu_hlt(void) { __asm__ volatile ("hlt"); }

/* sti immediately followed by hlt, in ONE asm statement.
 * The CPU delays interrupt delivery until after the instruction following sti,
 * so a wakeup cannot slip in between the two and be lost.
 * This is the only race-free way to sleep until an interrupt. Use it as:
 *     cpu_cli(); while (!condition) { cpu_sti_hlt(); cpu_cli(); } ...
 */
static inline void cpu_sti_hlt(void) { __asm__ volatile ("sti; hlt" ::: "memory"); }

#define CPU_RFLAGS_IF (1ull << 9)

/* Disable interrupts and return the previous RFLAGS;
 * give that value back to cpu_irq_restore() to re-enable ONLY if they were enabled before.
 * This is what a function should use instead of a bare cpu_sti():
 * it must not turn interrupts on behind a caller that had them off. */
static inline uint64_t cpu_irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void cpu_irq_restore(uint64_t flags) {
    if (flags & CPU_RFLAGS_IF)
        cpu_sti();
}

static inline uint64_t read_cr2(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(v));
    return v;
}

/* Descriptor-table register image used by lgdt / lidt. */
struct dt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static inline void cpu_lgdt(const struct dt_ptr *p) {
    __asm__ volatile ("lgdt %0" :: "m"(*p) : "memory");
}

static inline void cpu_lidt(const struct dt_ptr *p) {
    __asm__ volatile ("lidt %0" :: "m"(*p) : "memory");
}

static inline void cpu_ltr(uint16_t selector) {
    __asm__ volatile ("ltr %0" :: "r"(selector) : "memory");
}

static inline uint64_t read_cr3(void) {
    uint64_t v;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void write_cr3(uint64_t v) {
    __asm__ volatile ("mov %0, %%cr3" :: "r"(v) : "memory");
}

static inline void invlpg(const void *addr) {
    __asm__ volatile ("invlpg (%0)" :: "r"(addr) : "memory");
}

/* Stop this CPU for good. Interrupts off first so nothing can wake it. */
__attribute__((noreturn))
static inline void cpu_halt_forever(void) {
    cpu_cli();
    for (;;)
        __asm__ volatile ("hlt");
}

/* CPUID handler */
static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                        uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile ("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                              : "a"(leaf), "c"(subleaf));
}

/* PCI handlers */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

#endif
