#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <core/cpu.h>
#include <core/isr.h>
#include <core/panic.h>
#include <core/timer.h>
#include <core/kprintf.h>

#define PIT_FREQ  1193182ull        /* Hz, the PIT's input clock */
#define PIT_CH0   0x40
#define PIT_CMD   0x43

static volatile uint64_t ticks;
static uint64_t divisor = 1;
static bool inited;

/* Runs in IRQ context: count and nothing else. */
static void timer_irq(void *ctx) {
    (void)ctx;
    ticks++;
}

void timer_init(uint32_t hz) {
    if (hz == 0)
        hz = TIMER_HZ;
    uint64_t d = PIT_FREQ / hz;
    if (d < 1)     d = 1;
    if (d > 65535) d = 65535;       /* 16-bit counter */
    divisor = d;

    outb(PIT_CMD, 0x34);            /* channel 0, lobyte/hibyte, mode 2 (rate generator) */
    outb(PIT_CH0, (uint8_t)(d & 0xFF));
    outb(PIT_CH0, (uint8_t)(d >> 8));

    if (irq_register(0, timer_irq, NULL) != 0)
        PANIC("timer: IRQ0 already taken");
    inited = true;
    kprintf("→ timer_init() 16-bit counter, channel 0, lobyte/hibyte, mode 2 (rate generator)\n");
}

uint32_t timer_hz(void) {
    return (uint32_t)((PIT_FREQ + divisor / 2) / divisor);
}

uint64_t timer_ticks(void) {
    return ticks;
}

uint64_t uptime_ms(void) {
    return ticks * divisor * 1000ull / PIT_FREQ;
}

void timer_sleep_ms(uint64_t ms) {
    if (!inited || ms == 0)
        return;
    if (ms > 1000000000000ull)      /* keep the arithmetic below from overflowing */
        ms = 1000000000000ull;

    uint64_t need = (ms * PIT_FREQ + divisor * 1000ull - 1) / (divisor * 1000ull);   /* round up */

    uint64_t flags = cpu_irq_save();
    uint64_t target = ticks + need;
    while (ticks < target) {
        cpu_sti_hlt();
        cpu_cli();
    }
    cpu_irq_restore(flags);
}
