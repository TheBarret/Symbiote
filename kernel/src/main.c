#include <stddef.h>
#include <stdint.h>
#include <core/boot.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/kprintf.h>
#include <core/serial.h>
#include <core/shell.h>
#include <core/version.h>

void kmain(void) {
    if (!boot_protocol_ok())
        cpu_halt_forever();

    serial_init();
    kprintf("[kernel] Symbiote %s\n", SYM_VERSION);
    ext_init_all();
    kprintf("\n\x1b[1;92m[kernel] \x1b[0m %zu extension(s) loaded\n", ext_count());

    /* Hand control to the interactive shell. */
    shell_run();
}
