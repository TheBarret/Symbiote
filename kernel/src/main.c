#include <stddef.h>
#include <stdint.h>
#include <core/version.h>   // Master version, and also references config.h
#include <core/serial.h>    // Serial driver, read-only, default is COM1, 115200 baud
#include <core/boot.h>      // Limine API vectors and helpers
#include <core/cpu.h>       // CPU tables, CPUID reader
#include <core/kprintf.h>   // Formatted output to every console
#include <core/klog.h>      // Kernel Logger extension (kern, info, warn, error)
#include <core/pmm.h>       // Physical memory manager
#include <core/cmd.h>       // Global (Shell) Command dispatcher
#include <core/gdt.h>       // Global Descriptor Table, everything is ring 0
#include <core/heap.h>      // Block headers, Immediate coalescing, pmm_alloc_contig via HHDM
#include <core/idt.h>       // Interrupt Descriptor Table, 256 vectors
#include <core/isr.h>       // Interrupt Service Routine (ISR), isr_stubs.S stub wrapper
#include <core/pic.h>       // 8259 PIC Driver, virtual wires (LAPIC), interrupt platform
#include <core/timer.h>     // Clock controller, sleep and wait functions, default is 1Khz
#include <core/vmm.h>       // Virtual memory manager, ownership with PMM-backed frames
#include <core/vfs.h>       // Virtual File System, memory-based or ramfs
#include <core/ext.h>       // Extension Manager, loads custom /ext/* modules
#include <core/shell.h>     // Shell Manager, terminal control, adds extension apps, see ext.h and cmd.h

void kmain(void) {
    // check
    if (!boot_protocol_ok()) {
        cpu_halt_forever();
    }

    // bring up essentials
    serial_init();

    // init logger, present greeters and stages
    klog_init();
    klog_info("Loading kernel: Symbiote %s-%s...\n", SYM_VERSION, SYM_VERSION_INFO);

    // CPU tables (no heap required)
    gdt_init();
    idt_init();

    pmm_init();                 // memory
    struct pmm_stats pm = pmm_get_stats();
    klog_info("PMM: (%zu MiB usable)\n", (pm.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));

    heap_init();                // memory
    struct heap_stats hs = heap_get_stats();
    klog_info("HEAP: (%zu KiB free)\n", hs.bytes_free / 1024);

    vmm_init();                 // memory

    pic_init();                 // interrupts
    timer_init(TIMER_HZ);       // interrupts
    cpu_sti();                  // interrupts

    vfs_init();                 // filesystem

    klog_info("Stage 1 → Stage 2\n");

    ext_init_all();             // extensions
    klog_info("Stage 2 complete (ext: %zu, sys: %zu)\n", ext_count(), cmd_count());

    shell_run();                // present shell
}
