#include <stddef.h>
#include <stdint.h>
#include <core/boot.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/cmd.h>
#include <core/heap.h>
#include <core/klog.h>
#include <core/kprintf.h>
#include <core/pmm.h>
#include <core/serial.h>
#include <core/shell.h>
#include <core/version.h>
#include <core/vmm.h>

void kmain(void) {
    // check
    if (!boot_protocol_ok()) {
        cpu_halt_forever();
    }

    // bring up essentials
    serial_init();

    // present information
    klog("Symbiote %s\n", SYM_VERSION);
    klog("boot protocol...ok\n");
    klog("serial console...ok\n");

    ext_init_all();

    klog("extensions (shared: %zu, system: %zu)...ok\n", ext_count(), cmd_count());

    pmm_init();

    struct pmm_stats pm = pmm_get_stats();
    klog("physical memory (%zu MiB usable)...ok\n",
         (pm.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));

    heap_init();

    struct heap_stats hs = heap_get_stats();
    klog("kernel heap (%zu KiB free)...ok\n", hs.bytes_free / 1024);

    vmm_init();

    klog("virtual memory...ok\n");

    shell_run();
}
