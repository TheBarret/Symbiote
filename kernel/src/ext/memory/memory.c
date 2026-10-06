#include <core/boot.h>
#include <core/cmd.h>
#include <core/kprintf.h>
#include <core/pmm.h>

/* Memory */

/*  memmap helpers  */

static const char *memmap_type_name(uint64_t type) {
    switch (type) {
    case LIMINE_MEMMAP_USABLE:                 return "usable";
    case LIMINE_MEMMAP_RESERVED:               return "reserved";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "acpi-reclaim";
    case LIMINE_MEMMAP_ACPI_NVS:               return "acpi-nvs";
    case LIMINE_MEMMAP_BAD_MEMORY:             return "bad";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "boot-reclaim";
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return "kernel";
    case LIMINE_MEMMAP_FRAMEBUFFER:            return "framebuffer";
    case LIMINE_MEMMAP_RESERVED_MAPPED:        return "reserved-mapped";
    default:                                   return "unknown";
    }
}

/*  mem  */

static int cmd_mem_fn(const struct cmd_args *a) {
    (void)a;

    if (!boot_memory_ok()) {
        kprintf("Error: bootloader did not provide (memory) memmap or hhdm\n");
        return 1;
    }

    struct limine_memmap_response *map = boot_memmap();
    uint64_t hhdm = boot_hhdm_offset();

    uint64_t total_usable = 0;
    size_t usable_regions = 0;
    for (size_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE) {
            total_usable += e->length;
            usable_regions++;
        }
    }

    kprintf("Memory map: %zu region(s), %zu usable\n",
            map->entry_count, usable_regions);
    kprintf("  hhdm offset:   0x%llx\n", (unsigned long long)hhdm);
    kprintf("  usable total:  %llu KiB (%llu MiB)\n",
            (unsigned long long)(total_usable / 1024),
            (unsigned long long)(total_usable / (1024 * 1024)));
    return CMD_OK;
}

SYM_COMMAND(mem, "", "Memory status", cmd_mem_fn);

/*  memmap  */

static int cmd_memmap_fn(const struct cmd_args *a) {
    (void)a;

    if (!boot_memory_ok()) {
        kprintf("Error: bootloader did not provide (memory) memmap or hhdm\n");
        return 1;
    }

    struct limine_memmap_response *map = boot_memmap();
    kprintf("Memmap: %zu region(s), hhdm=0x%llx\n",
            map->entry_count,
            (unsigned long long)boot_hhdm_offset());

    for (size_t i = 0; i < map->entry_count; i++) {
        const struct limine_memmap_entry *e = map->entries[i];
        uint64_t start = e->base;
        uint64_t end   = e->base + e->length;
        kprintf("  [%zu] %016llx-%016llx  %-16s  %llu bytes\n",
                i,
                (unsigned long long)start,
                (unsigned long long)end,
                memmap_type_name(e->type),
                (unsigned long long)e->length);
    }
    return CMD_OK;
}

SYM_COMMAND(memmap, "", "Dump memory map", cmd_memmap_fn);

/*  pages  */

static int cmd_pages_fn(const struct cmd_args *a) {
    (void)a;
    struct pmm_stats s = pmm_get_stats();
    kprintf("Frames: %zu total, %zu free (%zu MiB free)\n",
            s.total_frames, s.free_frames,
            (s.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));
    return CMD_OK;
}

SYM_COMMAND(pages, "", "Physical frame allocator status", cmd_pages_fn);

/*  memtest  */

static int cmd_memtest_fn(const struct cmd_args *a) {
    (void)a;

    struct pmm_stats before = pmm_get_stats();
    size_t allocated = 0;

    /* Cap at a few thousand frames so the test doesn't take forever on
     * a machine with gigabytes of RAM. */
    size_t cap = before.free_frames;
    if (cap > 4096)
        cap = 4096;

    /* Use the pages themselves as an intrusive list, since we can't
     * store the addresses in a local array on the stack. */
    uint64_t head = 0;

    for (size_t i = 0; i < cap; i++) {
        uint64_t p = pmm_alloc();
        if (p == 0)
            break;

        if ((p & (PMM_PAGE_SIZE - 1)) != 0) {
            kprintf("memtest: misaligned frame 0x%llx\n",
                    (unsigned long long)p);
            return 1;
        }
        size_t frame = (size_t)(p / PMM_PAGE_SIZE);
        if (frame >= before.total_frames) {
            kprintf("Memtest: out-of-range frame 0x%llx\n",
                    (unsigned long long)p);
            return 1;
        }

        uint64_t *slot = (uint64_t *)pmm_phys_to_virt(p);
        *slot = head;
        head = p;
        allocated++;
    }

    if (allocated == 0) {
        kprintf("Memtest: could not allocate any frames\n");
        return 1;
    }

    size_t freed = 0;
    uint64_t node = head;
    while (node != 0) {
        uint64_t *slot = (uint64_t *)pmm_phys_to_virt(node);
        uint64_t next = *slot;
        pmm_free(node, 1);
        freed++;
        node = next;
    }

    struct pmm_stats after = pmm_get_stats();
    if (after.free_frames != before.free_frames) {
        kprintf("Memtest: leak or double free: before=%zu after=%zu\n",
                before.free_frames, after.free_frames);
        return 1;
    }

    kprintf("Memtest: %zu frames round-tripped, free count consistent\n",
            freed);
    return CMD_OK;
}

SYM_COMMAND(memtest, "", "PMM self-test (alloc/stamp/free round-trip)", cmd_memtest_fn);
