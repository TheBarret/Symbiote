#include <stddef.h>
#include <core/cpu.h>
#include <core/host.h>
#include <core/klog.h>
#include <core/ext.h>
#include <core/boot.h>
#include "configure.h"

/* Probe function shape.
 *
 * Contract:
 *   - return 0 on success, having filled its fields completely;
 *   - return non-zero on absence or failure, having written nothing.
 * A probe must not allocate heap memory that outlives it. Static pools or bootloader-owned pointers only. */
typedef int (*probe_fn)(void);

/* Each probe is declared here. Adding a probe means adding a prototype,
 * a definition in its own file, and one line in configure_run. */
static int probe_cpuid(void);
static int probe_boot(void);
/* static int probe_serial(void); */
/* static int probe_ps2(void); */
/* static int probe_acpi(void); */
/* static int probe_pci(void); */

/* Run one probe, log the outcome. Returns the probe's return value. */
static int run_probe(const char *name, probe_fn fn) {
    int rc = fn();
    if (rc != 0)
        klog_warning("probe: %-8s absent\n", name);
    else
        klog_info("probe: %-8s ok\n", name);
    return rc;
}

void configure_run(void) {
    klog_info("probing machine...\n");
    run_probe("cpuid",  probe_cpuid);
    run_probe("boot",   probe_boot);
    // TODO:
    /* run_probe("serial", probe_serial); */
    /* run_probe("ps2",    probe_ps2);    */
    /* run_probe("acpi",   probe_acpi);   */
    /* run_probe("pci",    probe_pci);    */
}

/* probe_cpuid
 Pure read: no allocation, nothing to free, cannot leak. Fills the
 CPU fields or returns non-zero if CPUID itself is unavailable
 (which on x86_64 it never is, but the contract is uniform).
 */

static int probe_cpuid(void) {
    uint32_t a, b, c, d;

    /* Leaf 0: vendor + max basic leaf. */
    uint32_t max_leaf;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    max_leaf = a;
    if (max_leaf < 1)
        return -1;

    /* Leaf 1: family/model, feature bits. */
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));

    uint32_t base_family = (a >> 8)  & 0x0F;
    uint32_t base_model  = (a >> 4)  & 0x0F;
    uint32_t ext_family  = (a >> 20) & 0xFF;
    uint32_t ext_model   = (a >> 16) & 0x0F;
    uint32_t family = base_family;
    uint32_t model  = base_model;
    if (base_family == 0x0F)
        family += ext_family;
    if (base_family == 0x06 || base_family == 0x0F)
        model += (ext_model << 4);

    /* Publish into locals first; nothing touches host.* until the end. */
    bool has_apic = (d >> 9)  & 1u;
    bool has_tsc  = (d >> 4)  & 1u;
    bool has_msr  = (d >> 5)  & 1u;
    bool has_sse  = (d >> 25) & 1u;
    bool has_sse2 = (d >> 26) & 1u;

    /* AVX requires leaf 1 ECX.28 and OSXSAVE, but for the state vector
     * we record the CPU's capability, not whether we enabled it. */
    bool has_avx = (c >> 28) & 1u;

    /* TSC frequency: leaf 0x15 if present. If ECX == 0 the ratio is unknown;
     * the .c side of cpu_tsc_hz() carries the family/model fallback, so we just record whatever we can get here. */
    uint64_t tsc_hz = 0;
    if (max_leaf >= 0x15) {
        uint32_t la, lb, lc, ld;
        __asm__ volatile ("cpuid" : "=a"(la), "=b"(lb), "=c"(lc), "=d"(ld) : "a"(0x15), "c"(0));
        if (lc != 0 && lb != 0)
            tsc_hz = (uint64_t)lc * lb;
    }
    if (tsc_hz == 0)
        tsc_hz = cpu_tsc_hz();      /* family/model fallback, may still be 0 */

    /* Publish. */
    host.cpu_has_apic = has_apic;
    host.cpu_has_tsc  = has_tsc;
    host.cpu_has_msr  = has_msr;
    host.cpu_has_sse  = has_sse;
    host.cpu_has_sse2 = has_sse2;
    host.cpu_has_avx  = has_avx;
    host.cpu_family   = family;
    host.cpu_model    = model;
    host.cpu_tsc_hz   = tsc_hz;
    return 0;
}

/* probe_boot
 *
 * Copies the facts Limine already handed us into the host state.
 * Nothing here probes hardware. The values are either present or they aren't, decided by the bootloader, not by us.
 *
 * No allocation. Every pointer stored here refers to Limine-owned memory, which stays mapped for the life of the kernel. */

int probe_boot(void) {
    /* Locals first. */
    uint64_t hhdm     = 0;
    uint32_t fb_w     = 0;
    uint32_t fb_h     = 0;
    uint32_t fb_pitch = 0;
    void    *fb_addr  = NULL;
    size_t   modcount = 0;

    /* HHDM offset: the higher-half direct map base Limine set up.
     * Returns 0 if unavailable, in which case nothing else can be
     * dereferenced, and we treat that as failure. */
    hhdm = boot_hhdm_offset();
    if (hhdm == 0)
        return -1;

    /* Framebuffer: present on most boots, absent on headless serial
     * boots. If absent, the getter returns NULL and we leave the
     * fb_* locals at their defaults; the probe still succeeds. */
    struct limine_framebuffer *fb = boot_framebuffer();
    if (fb) {
        fb_w     = fb->width;
        fb_h     = fb->height;
        fb_pitch = fb->pitch;
        fb_addr  = fb->address;
    }

    /* Boot modules: localfs, initrd, whatever Limine was given.
     * boot_modules sets *count and returns NULL if none. */
    uint64_t n = 0;
    (void)boot_modules(&n);
    modcount = (size_t)n;

    /* Publish. */
    host.boot_present      = true;
    host.boot_hhdm_offset  = hhdm;
    host.boot_fb_width     = fb_w;
    host.boot_fb_height    = fb_h;
    host.boot_fb_pitch     = fb_pitch;
    host.boot_fb_addr      = fb_addr;
    host.boot_module_count = modcount;
    return 0;
}
