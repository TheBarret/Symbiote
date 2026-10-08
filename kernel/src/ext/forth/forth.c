#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/boot.h>
#include <core/cmd.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/pmm.h>
#include <core/vmm.h>
#include <core/ext.h>
#include <core/heap.h>
#include <core/kprintf.h>
#include <lib/mem.h>
#include <lib/string.h>

/* Micro Forth.
 *
 * A tiny stack-based evaluator that runs a single line of Forth source.
 * No dictionary, no user-defined words, no control flow. Just enough for arithmetic and scripted automation.
 *
 * Two interfaces:
 *   - 'forth <expression>' shell command
 *   - At boot, running startup scripts
 */

#define FORTH_STACK_SIZE 256

typedef struct {
    int32_t data[FORTH_STACK_SIZE];
    int sp;
} forth_vm_t;

static void forth_init_vm(forth_vm_t *vm) {
    vm->sp = 0;
}

static void forth_push(forth_vm_t *vm, int32_t val) {
    if (vm->sp < FORTH_STACK_SIZE)
        vm->data[vm->sp++] = val;
    else
        kprintf("Warning: forth encountered a stack overflow\n");
}

static int32_t forth_pop(forth_vm_t *vm) {
    if (vm->sp > 0)
        return vm->data[--vm->sp];
    kprintf("Warning: forth encountered a stack underflow\n");
    return 0;
}

/* Parse a decimal integer, optionally negative.
 * Returns false on any non-numeric character, no overflow detection:
 * values wrap around int32_t, which matches typical Forth behavior. */
static bool parse_int(const char *str, int32_t *out) {
    int32_t val = 0;
    bool neg = false;
    if (*str == '-') {
        neg = true;
        str++;
    }
    if (!*str)
        return false;
    while (*str) {
        if (*str < '0' || *str > '9')
            return false;
        val = val * 10 + (*str - '0');
        str++;
    }
    *out = neg ? -val : val;
    return true;
}

/* Evaluate `len` bytes of Forth source, the source buffer is treated as read-only;
 * a private copy is made because the tokenizer writes NULs. */
static void forth_eval(const char *source, size_t len) {
    forth_vm_t vm;
    forth_init_vm(&vm);

    char *buf = kmalloc(len + 1);
    if (!buf) {
        kprintf("Warning: forth has ran out of memory for an evaluation buffer\n");
        return;
    }
    memcpy(buf, source, len);
    buf[len] = '\0';

    char *cursor = buf;
    while (*cursor) {
        while (*cursor == ' ' || *cursor == '\t' ||
               *cursor == '\n' || *cursor == '\r')
            cursor++;
        if (!*cursor)
            break;

        char *start = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t' &&
               *cursor != '\n' && *cursor != '\r')
            cursor++;
        char saved = *cursor;
        *cursor = '\0';

        if (strcmp(start, "+") == 0) {
            int32_t b = forth_pop(&vm);
            int32_t a = forth_pop(&vm);
            forth_push(&vm, a + b);
        } else if (strcmp(start, "-") == 0) {
            int32_t b = forth_pop(&vm);
            int32_t a = forth_pop(&vm);
            forth_push(&vm, a - b);
        } else if (strcmp(start, "*") == 0) {
            int32_t b = forth_pop(&vm);
            int32_t a = forth_pop(&vm);
            forth_push(&vm, a * b);
        } else if (strcmp(start, ".") == 0) {
            int32_t val = forth_pop(&vm);
            kprintf("%d ", val);
        } else if (strcmp(start, "emit") == 0) {
            int32_t val = forth_pop(&vm);
            kprintf("%c", (char)val);
        } else if (strcmp(start, "cr") == 0) {
            kprintf("\n");
        } else if (strcmp(start, "dup") == 0) {
            if (vm.sp > 0)
                forth_push(&vm, vm.data[vm.sp - 1]);
        } else if (strcmp(start, "drop") == 0) {
            forth_pop(&vm);
        } else if (strcmp(start, "exts") == 0) {            // extension count (shared module)
            forth_push(&vm, (int32_t)ext_count());
        } else if (strcmp(start, "cmds") == 0) {            // extension count (system module)
            forth_push(&vm, (int32_t)cmd_count());
        } else if (strcmp(start, "pages-free") == 0) {      // page free count
            struct pmm_stats s = pmm_get_stats();
            forth_push(&vm, (int32_t)(s.free_frames & 0x7fffffff));
        } else if (strcmp(start, "heap-used") == 0) {       // heap used count
            struct heap_stats s = heap_get_stats();
            forth_push(&vm, (int32_t)s.bytes_used);
        } else {
            int32_t num = 0;
            if (parse_int(start, &num))
                forth_push(&vm, num);
            else
                kprintf("forth: unknown word '%s'\n", start);
        }

        *cursor = saved;
        if (saved)
            cursor++;
    }

    kfree(buf);
}

/*  shell command  */
/* The spec is <code:str...>, so the dispatcher guarantees argc >= 1.
    * Join the arguments back into one line so Forth sees the spaces. */

static int cmd_forth_fn(const struct cmd_args *a) {
    char line[256];
    size_t n = 0;
    for (int i = 0; i < a->argc && n + 1 < sizeof line; i++) {
        if (i > 0 && n + 1 < sizeof line)
            line[n++] = ' ';
        const char *p = a->argv[i];
        while (*p && n + 1 < sizeof line)
            line[n++] = *p++;
    }
    line[n] = '\0';

    forth_eval(line, n);
    kprintf("\n");
    return CMD_OK;
}

SYM_COMMAND(forth, "<code:str...>", "Micro-Forth calculator (forth <expr>)", cmd_forth_fn);

/* Forth extension greeter */
static int forth_ext_init(void) { return 0; }
SYM_EXTENSION(mforth_calc, forth_ext_init, EXT_PRIO_APPLET);
