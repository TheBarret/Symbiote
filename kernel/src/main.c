#include <stddef.h>
#include <stdint.h>
#include <core/boot.h>
#include <core/cpu.h>
#include <core/ext.h>
#include <core/cmd.h>
#include <core/gdt.h>
#include <core/heap.h>
#include <core/idt.h>
#include <core/isr.h>
#include <core/klog.h>
#include <core/kprintf.h>
#include <core/pic.h>
#include <core/pmm.h>
#include <core/serial.h>
#include <core/shell.h>
#include <core/timer.h>
#include <core/version.h>
#include <core/vmm.h>
#include <core/vfs.h>

void kmain(void) {
    // check
    if (!boot_protocol_ok()) {
        cpu_halt_forever();
    }

    // bring up essentials
    serial_init();

    // init logger, present greeters and stages
    klog_init();
    klog_info("Loading kernel: Symbiote %s...\n", SYM_VERSION);

    // CPU tables (no heap required)
    gdt_init();
    idt_init();

    pmm_init();
    struct pmm_stats pm = pmm_get_stats();
    klog_info("PMM: (%zu MiB usable)\n", (pm.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));

    heap_init();
    struct heap_stats hs = heap_get_stats();
    klog_info("HEAP: (%zu KiB free)\n", hs.bytes_free / 1024);

    vmm_init();
    pic_init();                 // interrupt stage
    timer_init(TIMER_HZ);
    cpu_sti();
    vfs_init();

    klog_info("Stage 1 → Stage 2\n");

    ext_init_all();
    klog_info("Stage 2 complete (ext: %zu, sys: %zu)\n", ext_count(), cmd_count());

    shell_run();
}
