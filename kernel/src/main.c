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

void kmain(void) {
    // check
    if (!boot_protocol_ok()) {
        cpu_halt_forever();
    }

    // bring up essentials
    serial_init();

    // present greeters and stages
    klog("Loading kernel: Symbiote %s...\n", SYM_VERSION);

    // CPU tables (no heap required)
    gdt_init();
    idt_init();
    klog("GDT + TSS / IDT (vectors=256)...OK\n");

    pmm_init();
    struct pmm_stats pm = pmm_get_stats();
    klog("PMM (%zu MiB usable)...OK\n",
         (pm.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));

    heap_init();
    struct heap_stats hs = heap_get_stats();
    klog("HEAP (%zu KiB free)...OK\n", hs.bytes_free / 1024);

    vmm_init();
    klog("VMM...OK\n");

    pic_init();
    klog("8259 PIC (legacy) → LAPIC...OK\n");

    timer_init(TIMER_HZ);
    klog("Timer (%u Hz)...OK\n", timer_hz());

    cpu_sti();
    klog("Interrupts enabled\n");

    ext_init_all();
    klog("extensions (shared: %zu, system: %zu)...ok\n", ext_count(), cmd_count());

    shell_run();
}
