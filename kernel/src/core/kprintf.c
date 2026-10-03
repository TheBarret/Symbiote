#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <core/console.h>
#include <core/kprintf.h>
#include <lib/string.h>

/* The formatter produces characters one at a time into an "emit" callback,
 * so the same code feeds both the consoles and string buffers. */
typedef void (*emit_fn)(void *ctx, char c);

static void emit_repeat(emit_fn emit, void *ctx, char c, size_t n) {
    for (size_t i = 0; i < n; i++)
        emit(ctx, c);
}

/* Print prefix + body, padded out to `width`.
 * Zero padding goes between the* prefix (sign, "0x") and the digits,
 * which is what you expect from %08x. */
static void emit_field(emit_fn emit, void *ctx, const char *prefix, size_t plen,
                       const char *body, size_t blen,
                       size_t width, bool left, bool zero) {
    size_t total = plen + blen;
    size_t pad = width > total ? width - total : 0;

    if (!left && !zero)
        emit_repeat(emit, ctx, ' ', pad);
    for (size_t i = 0; i < plen; i++)
        emit(ctx, prefix[i]);
    if (!left && zero)
        emit_repeat(emit, ctx, '0', pad);
    for (size_t i = 0; i < blen; i++)
        emit(ctx, body[i]);
    if (left)
        emit_repeat(emit, ctx, ' ', pad);
}

/* Write `v` in `base` into the END of tmp; returns the start of the digits. */
static char *utoa_end(unsigned long long v, unsigned base, bool upper, char *end) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char *p = end;
    do {
        *--p = digits[v % base];
        v /= base;
    } while (v);
    return p;
}

static void format(emit_fn emit, void *ctx, const char *fmt, va_list ap) {
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            emit(ctx, *fmt);
            continue;
        }
        fmt++;

        bool left = false, zero = false;
        for (;; fmt++) {
            if (*fmt == '-') left = true;
            else if (*fmt == '0') zero = true;
            else break;
        }

        size_t width = 0;
        if (*fmt == '*') {
            int w = va_arg(ap, int);
            if (w < 0) { left = true; w = -w; }
            width = (size_t)w;
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (size_t)(*fmt++ - '0');
        }

        bool is_long = false;           /* l, ll and z are all 64-bit here */
        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
            if (*fmt != 'h') is_long = true;
            fmt++;
        }

        char tmp[32];
        char *end = tmp + sizeof tmp;

        switch (*fmt) {
        case 'c': {
            char c = (char)va_arg(ap, int);
            emit_field(emit, ctx, "", 0, &c, 1, width, left, false);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            emit_field(emit, ctx, "", 0, s, strlen(s), width, left, false);
            break;
        }
        case 'd':
        case 'i': {
            long long v = is_long ? va_arg(ap, long long) : va_arg(ap, int);
            bool neg = v < 0;
            unsigned long long u = neg ? 0ULL - (unsigned long long)v : (unsigned long long)v;
            char *p = utoa_end(u, 10, false, end);
            emit_field(emit, ctx, neg ? "-" : "", neg ? 1 : 0, p, (size_t)(end - p),
                       width, left, zero);
            break;
        }
        case 'u':
        case 'x':
        case 'X': {
            unsigned long long v = is_long ? va_arg(ap, unsigned long long)
                                           : va_arg(ap, unsigned int);
            unsigned base = (*fmt == 'u') ? 10 : 16;
            char *p = utoa_end(v, base, *fmt == 'X', end);
            emit_field(emit, ctx, "", 0, p, (size_t)(end - p), width, left, zero);
            break;
        }
        case 'p': {
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            char *p = utoa_end(v, 16, false, end);
            emit_field(emit, ctx, "0x", 2, p, (size_t)(end - p), width, left, zero);
            break;
        }
        case '%':
            emit(ctx, '%');
            break;
        case '\0':
            return;                     /* format string ended after a '%' */
        default:                        /* unknown conversion: show it as-is */
            emit(ctx, '%');
            emit(ctx, *fmt);
            break;
        }
    }
}

/* ---- sink 1: the consoles (buffered, flushed in chunks) ---- */
struct con_sink {
    char buf[128];
    size_t len;
};

static void con_flush(struct con_sink *s) {
    if (s->len) {
        console_write(s->buf, s->len);
        s->len = 0;
    }
}

static void con_emit(void *ctx, char c) {
    struct con_sink *s = ctx;
    s->buf[s->len++] = c;
    if (s->len == sizeof s->buf)
        con_flush(s);
}

void kvprintf(const char *fmt, va_list ap) {
    struct con_sink s = { .len = 0 };
    format(con_emit, &s, fmt, ap);
    con_flush(&s);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

/* ---- sink 2: a caller-supplied string buffer ---- */
struct str_sink {
    char *buf;
    size_t cap;
    size_t len;     /* counts every character, even ones that did not fit */
};

static void str_emit(void *ctx, char c) {
    struct str_sink *s = ctx;
    if (s->cap && s->len + 1 < s->cap)
        s->buf[s->len] = c;
    s->len++;
}

int kvsnprintf(char *buf, size_t cap, const char *fmt, va_list ap) {
    struct str_sink s = { buf, cap, 0 };
    format(str_emit, &s, fmt, ap);
    if (cap)
        buf[s.len < cap ? s.len : cap - 1] = '\0';
    return (int)s.len;
}

int ksnprintf(char *buf, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return n;
}
