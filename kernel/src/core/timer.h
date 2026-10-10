#ifndef CORE_TIMER_H
#define CORE_TIMER_H

#include <stdint.h>
#include "core/version.h"

// Moved to ref: core/version.h
//#define TIMER_HZ 1000

/* PIT channel 0 as the system tick. Needs idt_init() and pic_init();
 * the tick only starts arriving once interrupts are enabled with cpu_sti(). */
void timer_init(uint32_t hz);

/* The real tick rate (the PIT divisor is an integer, so it is not exactly hz). */
uint32_t timer_hz(void);

uint64_t timer_ticks(void);

/* Milliseconds since timer_init, computed from the actual divisor so it does not drift (1193182 does not divide evenly by 1000). */
uint64_t uptime_ms(void);

/* Sleep at least `ms` milliseconds with the CPU halted between ticks.
 * Interrupts must be able to run: do not call before cpu_sti(). */
void timer_sleep_ms(uint64_t ms);

#endif
