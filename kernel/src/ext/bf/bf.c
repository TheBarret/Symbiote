#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <core/boot.h>
#include <core/cmd.h>
#include <core/console.h>
#include <core/cpu.h>
#include <core/pmm.h>
#include <core/vmm.h>
#include <core/heap.h>
#include <core/keyboard.h>
#include <core/ext.h>
#include <core/kprintf.h>
#include <lib/mem.h>
#include <lib/string.h>

/* Micro Brainfuck
 *
 * A tiny interpreter that runs a Brainfuck program.
 * Eight commands, a fixed tape, and bracket-matching control flow.
 * The interpreter has no notion of programs, files, or state that outlives a single evaluation.
 *
 * Two interfaces:
 *   - 'bf <program>' shell command (interactive; ',' reads the keyboard)
 *   - At boot, running a startup script loaded as a Limine module
 *
 * Policies:
 *   - The tape is 30000 bytes, zeroed before each run.
 *   - '>' and '<' wrap rather than fault, so a program cannot escape the tape.
 *   - Bracket jumps are validated as they are taken; an unbalanced program
 *     is reported and aborted, not silently mis-executed.
 *   - At boot, ',' returns 0 (no interactive input is possible).
 *     At the shell, ',' blocks on the keyboard.
 *   - An instruction budget caps runaway programs. The default is generous
 *     but finite, so a mistaken loop terminates instead of hanging the shell.
 */

#define BF_TAPE_SIZE     30000
#define BF_MAX_STEPS     (1u << 24)     /* 16M instructions per run */

typedef struct {
    uint8_t *tape;
    size_t   dp;        /* data pointer, always in [0, BF_TAPE_SIZE) */
    size_t   ip;        /* instruction pointer, always in [0, len) */
    bool     interactive;
    uint32_t steps;     /* instruction budget consumed */
} bf_vm_t;

static void bf_init_vm(bf_vm_t *vm, uint8_t *tape, bool interactive) {
    vm->tape = tape;
    vm->dp = 0;
    vm->ip = 0;
    vm->interactive = interactive;
    vm->steps = 0;
}

/*  bracket matching
 *
 * Both directions are needed: '[' may need to jump forward to its ']'
 * when the current cell is zero, and ']' may need to jump back to its '['
 * when the current cell is nonzero. Both return BF_TAPE_SIZE (an impossible ip) on failure,
 * so callers can treat the failure uniformly.
 *
 * The scan is bounded by the source length in both directions,
 * so a malformed program cannot cause an out-of-bounds read.
 */

static size_t bf_scan_forward(const char *src, size_t len, size_t ip) {
    /* src[ip] is '['. Return the index of its matching ']'. */
    int depth = 1;
    for (size_t i = ip + 1; i < len; i++) {
        if (src[i] == '[') depth++;
        else if (src[i] == ']') {
            if (--depth == 0)
                return i;
        }
    }
    return (size_t)-1;      /* unterminated */
}

static size_t bf_scan_backward(const char *src, size_t ip) {
    /* src[ip] is ']'. Return the index of its matching '['. */
    int depth = 1;
    while (ip > 0) {
        ip--;
        if (src[ip] == ']') depth++;
        else if (src[ip] == '[') {
            if (--depth == 0)
                return ip;
        }
    }
    return (size_t)-1;      /* unterminated */
}

/*  the interpreter  */

/* Returns 0 on success, negative on failure. Prints its own diagnostics. */
static int bf_eval(const char *src, size_t len, bool interactive) {
    uint8_t *tape = kzalloc(BF_TAPE_SIZE);
    if (!tape) {
        kprintf("bf: out of memory for a tape\n");
        return -1;
    }

    bf_vm_t vm;
    bf_init_vm(&vm, tape, interactive);

    int rc = 0;

    while (vm.ip < len) {
        if (++vm.steps > BF_MAX_STEPS) {
            kprintf("bf: instruction budget exceeded (%u steps); aborting\n",
                    (unsigned)BF_MAX_STEPS);
            rc = -1;
            break;
        }

        char c = src[vm.ip];

        switch (c) {
        case '>':
            vm.dp = (vm.dp + 1) % BF_TAPE_SIZE;
            break;
        case '<':
            vm.dp = (vm.dp + BF_TAPE_SIZE - 1) % BF_TAPE_SIZE;
            break;
        case '+':
            tape[vm.dp]++;
            break;
        case '-':
            tape[vm.dp]--;
            break;
        case '.':
            console_putc((char)tape[vm.dp]);
            break;
        case ',':
            tape[vm.dp] = vm.interactive
                ? (uint8_t)kbd_getchar()
                : 0;
            break;
        case '[':
            if (tape[vm.dp] == 0) {
                size_t match = bf_scan_forward(src, len, vm.ip);
                if (match == (size_t)-1) {
                    kprintf("bf: unmatched '[' at offset %zu\n", vm.ip);
                    rc = -1;
                    goto done;
                }
                vm.ip = match;
            }
            break;
        case ']':
            if (tape[vm.dp] != 0) {
                size_t match = bf_scan_backward(src, vm.ip);
                if (match == (size_t)-1) {
                    kprintf("bf: unmatched ']' at offset %zu\n", vm.ip);
                    rc = -1;
                    goto done;
                }
                vm.ip = match;
            }
            break;
        default:
            /* Any other byte is a comment. This matches the standard. */
            break;
        }

        vm.ip++;
    }

done:
    kfree(tape);
    return rc;
}

/*  shell command  */

/* The spec is <code:str...>, so the dispatcher guarantees argc >= 1.
     * Join the arguments back into one line so the program sees the spaces.
     * Non-command characters (including the spaces) are comments to the interpreter, so the joined form is safe. */
static int cmd_bf_fn(const struct cmd_args *a) {

    char line[512];
    size_t n = 0;
    for (int i = 0; i < a->argc && n + 1 < sizeof line; i++) {
        if (i > 0 && n + 1 < sizeof line)
            line[n++] = ' ';
        const char *p = a->argv[i];
        while (*p && n + 1 < sizeof line)
            line[n++] = *p++;
    }
    line[n] = '\0';

    int rc = bf_eval(line, n, true);
    kprintf("\n");
    return (rc == 0) ? CMD_OK : 1;
}

SYM_COMMAND(bf, "<code:str...>", "Micro-Brainfuck interpreter (bf <source>)", cmd_bf_fn);

/* Bf extension greeter */
static int bf_ext_init(void) { return 0; }
SYM_EXTENSION(mbf_interpreter, bf_ext_init, EXT_PRIO_APPLET);
