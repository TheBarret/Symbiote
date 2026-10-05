#ifndef CORE_CPU_H
#define CORE_CPU_H

#include <stdint.h>

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

static inline void cpu_cli(void) { __asm__ volatile ("cli"); }
static inline void cpu_sti(void) { __asm__ volatile ("sti"); }

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

#endif
