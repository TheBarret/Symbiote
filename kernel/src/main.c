#include <stddef.h>
#include <stdint.h>
#include <core/version.h>
#include <core/boot.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/kprintf.h>
#include <core/serial.h>
#include <core/shell.h>
#include <core/pmm.h>

void kmain(void) {
    if (!boot_protocol_ok())
        kprintf("Error: boot protocol failed!\n");
        cpu_halt_forever();

    kprintf("%s kernel version: %s\n", SYM_PREFIX, SYM_VERSION);

    serial_init();
    kprintf("%s serial port enabled\n", SYM_PREFIX);

    ext_init_all();
    kprintf("%s extensions enabled (loaded: \x1b[0m %zu)\n", SYM_PREFIX, ext_count());

    pmm_init();
    kprintf("%s memory manager enabled\n", SYM_PREFIX);

    /* Hand control to the interactive shell. */
    shell_run();
}
