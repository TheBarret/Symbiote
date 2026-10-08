#include <stddef.h>
#include <lib/string.h>
#include <lib/mem.h>
#include <core/boot.h>
#include <core/cmd.h>
#include <core/ext.h>
#include <core/kprintf.h>
#include <core/pmm.h>
#include <core/heap.h>
#include <core/vmm.h>

/*  mem info  */

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

static int cmd_mem_fn(const struct cmd_args *a) {
    (void)a;

    if (!boot_memory_ok()) {
        kprintf("mem: bootloader did not provide memmap or hhdm\n");
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

    kprintf("Memory\n");
    kprintf("  hhdm offset:   0x%llx\n", (unsigned long long)hhdm);
    kprintf("  usable total:  %llu KiB (%llu MiB)\n",
            (unsigned long long)(total_usable / 1024),
            (unsigned long long)(total_usable / (1024 * 1024)));

    struct pmm_stats p = pmm_get_stats();
    kprintf("  frames:        %zu total, %zu free (%zu MiB free)\n",
            p.total_frames, p.free_frames,
            (p.free_frames * PMM_PAGE_SIZE) / (1024 * 1024));

    struct heap_stats h = heap_get_stats();
    kprintf("  heap:          %zu chunks, %zu blocks (%zu free)\n",
            h.chunks, h.blocks, h.blocks_free);
    kprintf("  heap bytes:    %zu used, %zu free, largest %zu\n",
            h.bytes_used, h.bytes_free, h.largest_free);

    kprintf("Memmap: %zu region(s), %zu usable\n",
            map->entry_count, usable_regions);

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

SYM_COMMAND(mem, "", "Memory status", cmd_mem_fn);

/*  memtest  */

static int test_pmm_distinct(void) {
    enum { N = 16 };
    uint64_t frames[N];
    for (int i = 0; i < N; i++) {
        frames[i] = pmm_alloc();
        if (frames[i] == 0) {
            for (int j = 0; j < i; j++) pmm_free(frames[j], 1);
            kprintf("  pmm_distinct: FAIL (ran out at %d)\n", i);
            return 1;
        }
    }
    for (int i = 0; i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            if (frames[i] == frames[j]) {
                kprintf("  pmm_distinct: FAIL (duplicate 0x%llx)\n",
                        (unsigned long long)frames[i]);
                for (int k = 0; k < N; k++) pmm_free(frames[k], 1);
                return 1;
            }
        }
    }
    for (int i = 0; i < N; i++)
        pmm_free(frames[i], 1);
    kprintf("  pmm_distinct: ok\n");
    return 0;
}

static int test_pmm_align(void) {
    enum { N = 16 };
    uint64_t frames[N];
    for (int i = 0; i < N; i++) {
        frames[i] = pmm_alloc();
        if (frames[i] == 0) {
            for (int j = 0; j < i; j++) pmm_free(frames[j], 1);
            kprintf("  pmm_align: SKIP (ran out at %d)\n", i);
            return 0;
        }
        if ((frames[i] & (PMM_PAGE_SIZE - 1)) != 0) {
            kprintf("  pmm_align: FAIL (0x%llx not page-aligned)\n",
                    (unsigned long long)frames[i]);
            for (int k = 0; k <= i; k++) pmm_free(frames[k], 1);
            return 1;
        }
    }
    for (int i = 0; i < N; i++)
        pmm_free(frames[i], 1);
    kprintf("  pmm_align: ok\n");
    return 0;
}

static int test_pmm_contig(void) {
    enum { N = 8 };
    uint64_t base = pmm_alloc_contig(N);
    if (base == 0) {
        kprintf("  pmm_contig: SKIP (no %d-frame run available)\n", N);
        return 0;
    }
    pmm_free(base, N);
    uint64_t again = pmm_alloc_contig(N);
    if (again != base) {
        kprintf("  pmm_contig: WARN (re-allocated at 0x%llx, not 0x%llx)\n",
                (unsigned long long)again, (unsigned long long)base);
        pmm_free(again, N);
        return 0;
    }
    pmm_free(again, N);
    kprintf("  pmm_contig: ok\n");
    return 0;
}

static int test_pmm_roundtrip(void) {
    struct pmm_stats before = pmm_get_stats();
    size_t allocated = 0;

    size_t cap = before.free_frames;
    if (cap > 4096)
        cap = 4096;

    uint64_t head = 0;
    for (size_t i = 0; i < cap; i++) {
        uint64_t p = pmm_alloc();
        if (p == 0)
            break;

        if ((p & (PMM_PAGE_SIZE - 1)) != 0) {
            kprintf("  pmm_roundtrip: FAIL (misaligned frame 0x%llx)\n",
                    (unsigned long long)p);
            return 1;
        }
        size_t frame = (size_t)(p / PMM_PAGE_SIZE);
        if (frame >= before.total_frames) {
            kprintf("  pmm_roundtrip: FAIL (out-of-range frame 0x%llx)\n",
                    (unsigned long long)p);
            return 1;
        }

        uint64_t *slot = (uint64_t *)pmm_phys_to_virt(p);
        *slot = head;
        head = p;
        allocated++;
    }

    if (allocated == 0) {
        kprintf("  pmm_roundtrip: FAIL (could not allocate any frames)\n");
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
        kprintf("  pmm_roundtrip: FAIL (before=%zu after=%zu)\n",
                before.free_frames, after.free_frames);
        return 1;
    }

    kprintf("  pmm_roundtrip: ok (%zu frames)\n", freed);
    return 0;
}

static int test_heap_zero(void) {
    void *p = kzalloc(64);
    if (!p) {
        kprintf("  heap_zero: SKIP (kzalloc failed)\n");
        return 0;
    }
    uint8_t *bytes = p;
    for (int i = 0; i < 64; i++) {
        if (bytes[i] != 0) {
            kprintf("  heap_zero: FAIL (byte %d = 0x%02x)\n", i, bytes[i]);
            kfree(p);
            return 1;
        }
    }
    kfree(p);
    kprintf("  heap_zero: ok\n");
    return 0;
}

static int test_heap_null(void) {
    kfree(NULL);
    kprintf("  heap_null: ok\n");
    return 0;
}

static int test_heap_realloc(void) {
    char *p = kmalloc(16);
    if (!p) {
        kprintf("  heap_realloc: SKIP\n");
        return 0;
    }
    memcpy(p, "0123456789abcdef", 16);
    char *q = krealloc(p, 32);
    if (!q) {
        kprintf("  heap_realloc: FAIL (grow returned NULL)\n");
        kfree(p);
        return 1;
    }
    if (memcmp(q, "0123456789abcdef", 16) != 0) {
        kprintf("  heap_realloc: FAIL (contents lost on grow)\n");
        kfree(q);
        return 1;
    }
    char *r = krealloc(q, 8);
    if (!r) {
        kprintf("  heap_realloc: FAIL (shrink returned NULL)\n");
        kfree(q);
        return 1;
    }
    if (memcmp(r, "01234567", 8) != 0) {
        kprintf("  heap_realloc: FAIL (contents lost on shrink)\n");
        kfree(r);
        return 1;
    }
    kfree(r);
    kprintf("  heap_realloc: ok\n");
    return 0;
}

static int test_vmm_map_unmap(void) {
    uint64_t va = VMM_DYNAMIC_BASE;
    uint64_t frame = pmm_alloc();
    if (frame == 0) {
        kprintf("  vmm_map_unmap: SKIP\n");
        return 0;
    }
    if (!vmm_map(va, frame, VMM_WRITE | VMM_NX)) {
        kprintf("  vmm_map_unmap: FAIL (map returned false)\n");
        pmm_free(frame, 1);
        return 1;
    }
    if (vmm_translate(va) != frame) {
        kprintf("  vmm_map_unmap: FAIL (translate != frame)\n");
        vmm_unmap(va);
        pmm_free(frame, 1);
        return 1;
    }
    if (!vmm_unmap(va)) {
        kprintf("  vmm_map_unmap: FAIL (unmap returned false)\n");
        pmm_free(frame, 1);
        return 1;
    }
    if (vmm_translate(va) != 0) {
        kprintf("  vmm_map_unmap: FAIL (still mapped after unmap)\n");
        pmm_free(frame, 1);
        return 1;
    }
    pmm_free(frame, 1);
    kprintf("  vmm_map_unmap: ok\n");
    return 0;
}

static int cmd_memtest_fn(const struct cmd_args *a) {
    (void)a;

    struct test {
        const char *name;
        int (*fn)(void);
    };
    static const struct test tests[] = {
        { "pmm_distinct",    test_pmm_distinct },
        { "pmm_align",       test_pmm_align },
        { "pmm_contig",      test_pmm_contig },
        { "pmm_roundtrip",   test_pmm_roundtrip },
        { "heap_zero",       test_heap_zero },
        { "heap_null",       test_heap_null },
        { "heap_realloc",    test_heap_realloc },
        { "vmm_map_unmap",   test_vmm_map_unmap },
    };

    size_t ran = 0, failed = 0;
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        ran++;
        if (tests[i].fn() != 0)
            failed++;
    }

    kprintf("Memtest: %zu ran, %zu failed\n", ran, failed);
    return failed ? 1 : CMD_OK;
}

SYM_COMMAND(memtest, "", "Memory test", cmd_memtest_fn);

/*  Heap tester  */

static int cmd_heaptest_fn(const struct cmd_args *a) {
    /* Default 2000, the spec is [iterations:num]. */
    unsigned iterations = 2000;
    if (a->argc >= 1)
        iterations = (unsigned)a->num[0];

    enum { N = 64 };
    void *ptrs[N] = {0};
    size_t sizes[N] = {0};

    /* Deterministic seed so runs are reproducible. */
    uint32_t seed = 0x12345678;
    struct heap_stats before = heap_get_stats();

    for (unsigned iter = 0; iter < iterations; iter++) {
        seed = seed * 1103515245u + 12345u;

        int slot = (int)((seed >> 16) % N);
        if (ptrs[slot] == NULL) {
            size_t sz = 1 + ((seed >> 8) % 1024);
            void *p = kmalloc(sz);
            if (p == NULL)
                continue;
            uint8_t *bytes = p;
            for (size_t i = 0; i < sz; i++)
                bytes[i] = (uint8_t)(slot ^ i);
            ptrs[slot] = p;
            sizes[slot] = sz;
        } else {
            uint8_t *bytes = ptrs[slot];
            for (size_t i = 0; i < sizes[slot]; i++) {
                if (bytes[i] != (uint8_t)(slot ^ i)) {
                    kprintf("Heaptest: corruption in slot %d at byte %zu\n",
                            slot, i);
                    return 1;
                }
            }
            kfree(ptrs[slot]);
            ptrs[slot] = NULL;
            sizes[slot] = 0;
        }
    }

    /* Free whatever is left. */
    for (int i = 0; i < N; i++) {
        if (ptrs[i]) {
            kfree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    struct heap_stats after = heap_get_stats();

    /* Resident allocations are legitimate and are already in `before`.
     * A leak is anything this test left behind. */
    size_t live_before = before.blocks - before.blocks_free;
    size_t live_after  = after.blocks  - after.blocks_free;

    //if (after.bytes_used != 0) {
    //        kprintf("heaptest: leak: %zu bytes still used\n", after.bytes_used);
    //        return 1;
    //    }
    if (after.bytes_used != before.bytes_used || live_after != live_before) {
        kprintf("Heaptest: leak: used %zu -> %zu bytes, live blocks %zu -> %zu\n",
                before.bytes_used, after.bytes_used, live_before, live_after);
        return 1;
    }

    kprintf("Heaptest: %u iterations OK (chunks: %zu -> %zu, used: %zu bytes)\n",
            iterations, before.chunks, after.chunks, after.bytes_used);
    return CMD_OK;
}

SYM_COMMAND(heaptest, "[iterations:num]", "Heap test", cmd_heaptest_fn);

/* extension greeter */
static int memory_ext_init(void) { return 0; }
SYM_EXTENSION(memory_toolkit, memory_ext_init, EXT_PRIO_APPLET);
