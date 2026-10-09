#ifndef CORE_HOST_H
#define CORE_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host state.
 *
 * What the machine presented, observed once during the configure phase.
 * This is a *view*, not an owner: no field in here owns memory.
 * Pointers either refer to bootloader-owned data (Limine responses, RSDP) or
 * to static storage inside the probe that filled them.
 * No consumer should ever free anything reachable from here.
 *
 * A probe either fills its fields completely and returns 0,
 * or returns non-zero and leaves the fields exactly as they were.
 * There is no partially-populated state.
 *
 * Consumers read fields directly. They do not re-probe.
 * If a field is false or zero, the host does not have that thing. */

struct host_state {
    /*  CPUID  */

    /* Filled by probe_cpuid, always at least attempted. */
    bool     cpu_has_apic;
    bool     cpu_has_tsc;
    bool     cpu_has_msr;
    bool     cpu_has_sse;
    bool     cpu_has_sse2;
    bool     cpu_has_avx;
    uint32_t cpu_family;
    uint32_t cpu_model;
    uint64_t cpu_tsc_hz;        /* 0 if not discoverable */

    /*  Bootloader  */

    /* Filled by probe_boot, always present on x86_64 (Limine is the bootloader).
     * These are pointers into Limine-owned memory. */
    bool     boot_present;
    uint64_t boot_hhdm_offset;
    uint32_t boot_fb_width;
    uint32_t boot_fb_height;
    uint32_t boot_fb_pitch;
    void    *boot_fb_addr;
    size_t   boot_module_count;

    /*  Serial  */

    /* Filled by probe_serial. */
    bool     serial_present;
    bool     serial_com1;

    /*  PS/2  */

    /* Filled by probe_ps2. kbd and mouse reflect what the 8042 says it has.
     * Either can be false on a machine where the controller exists but only one port is wired. */
    bool     ps2_present;
    bool     ps2_kbd;
    bool     ps2_mouse;

    /*  ACPI  */

    /* Filled by probe_acpi. rsdp points into bootloader-provided memory. */
    bool     acpi_present;
    void    *acpi_rsdp;
    uint8_t  acpi_revision;
    uint32_t acpi_table_count;

    /*  PCI  */

    /* Filled by probe_pci. The device array is static storage inside probe_pci.c; host_state does not own it. */
    bool     pci_present;
    size_t   pci_device_count;
};

/* One global, zero-initialized; probes fill it in. */
extern struct host_state host;

#endif
