#include <stddef.h>
#include <stdint.h>
#include <core/boot.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/kprintf.h>
#include <core/serial.h>

#define SYM_VERSION "0.1.0"

/* The kernel entry point; the linker script names it as ENTRY. */
void kmain(void) {
    if (!boot_protocol_ok())
        cpu_halt_forever();

    /* Serial console */
    serial_init();
    kprintf("[boot] Symbiote %s\n", SYM_VERSION);

    /* Framebuffer console, drivers, shells go here.. */
    ext_init_all();

    kprintf("\n\x1b[1;92mSymbiote %s\x1b[0m  chassis ready, %zu extension(s) attached.\n", SYM_VERSION, ext_count());
    kprintf("Nothing else to do yet. This is milestone M1.\n");

    /* Idle, (interrupts are not set up until M2/M3, so this simply parks) */
    cpu_halt_forever();
}
