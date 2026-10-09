#include <stddef.h>
#include <core/cmd.h>
#include <core/host.h>
#include <core/kprintf.h>

/* Shell command: print the host state.
 *
 * Read-only. Anything a probe might have filled shows up here,
 * so the state vector is inspectable without a debugger. */

static int cmd_hostinfo_fn(const struct cmd_args *a) {
    (void)a;

    kprintf("Probe Report:\n");

    kprintf("  cpu:      family=%u model=%u apic=%d tsc=%d (%llu Hz) "
            "msr=%d sse=%d sse2=%d avx=%d\n",
            host.cpu_family, host.cpu_model,
            (int)host.cpu_has_apic, (int)host.cpu_has_tsc,
            (unsigned long long)host.cpu_tsc_hz,
            (int)host.cpu_has_msr, (int)host.cpu_has_sse,
            (int)host.cpu_has_sse2, (int)host.cpu_has_avx);

    kprintf("  boot:     present=%d hhdm=0x%llx fb=%ux%u pitch=%u addr=%p\n",
            (int)host.boot_present,
            (unsigned long long)host.boot_hhdm_offset,
            host.boot_fb_width, host.boot_fb_height, host.boot_fb_pitch,
            host.boot_fb_addr);

    kprintf("  serial:   present=%d\n", (int)host.serial_present);
    kprintf("  ps2:      present=%d\n", (int)host.ps2_present);
    kprintf("  acpi:     present=%d rsdp=%p\n", (int)host.acpi_present, host.acpi_rsdp);
    kprintf("  pci:      present=%d devices=%zu\n", (int)host.pci_present, host.pci_device_count);
    return 0;
}

SYM_COMMAND(probe, "", "Display probe info", cmd_hostinfo_fn);
