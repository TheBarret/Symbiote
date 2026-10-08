#include <core/cmd.h>
#include <core/heap.h>
#include <core/kprintf.h>

/* Heap */

static int cmd_heap_fn(const struct cmd_args *a) {
    (void)a;

    struct heap_stats s = heap_get_stats();
    kprintf("Heap: %zu chunk(s), %zu block(s) (%zu free)\n",
            s.chunks, s.blocks, s.blocks_free);
    kprintf("  used:         %zu bytes\n", s.bytes_used);
    kprintf("  free:         %zu bytes\n", s.bytes_free);
    kprintf("  largest free: %zu bytes\n", s.largest_free);
    return CMD_OK;
}

SYM_COMMAND(heap, "", "Heap status", cmd_heap_fn);

/*  heaptest  */

static int cmd_heaptest_fn(const struct cmd_args *a) {
    /* Optional iteration count. Default 2000, which is what the built-in
     * version used. The spec is [iterations:num]. */
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
    if (after.bytes_used != 0) {
        kprintf("heaptest: leak: %zu bytes still used\n", after.bytes_used);
        return 1;
    }

    kprintf("Heaptest: %u iterations OK (chunks: %zu -> %zu, used: %zu bytes)\n",
            iterations, before.chunks, after.chunks, after.bytes_used);
    return CMD_OK;
}

SYM_COMMAND(heaptest, "[iterations:num]", "Heap test", cmd_heaptest_fn);
