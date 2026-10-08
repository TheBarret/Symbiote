#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <core/console.h>
#include <core/heap.h>
#include <core/keyboard.h>
#include <core/kprintf.h>
#include <core/tui.h>
#include <lib/mem.h>
#include <lib/string.h>

struct cell {
    char    ch;
    uint8_t attr;
};

static struct cell *front;
static struct cell *back;
static int tui_w;
static int tui_h;

/* default = 80x25 */
#define TUI_W 80
#define TUI_H 25

bool tui_begin(void) {
    tui_w = TUI_W;
    tui_h = TUI_H;

    size_t cells = (size_t)tui_w * (size_t)tui_h;
    front = kmalloc(cells * sizeof *front);
    back  = kmalloc(cells * sizeof *back);
    if (!front || !back) {
        kfree(front);
        kfree(back);
        front = back = NULL;
        return false;
    }

    memset(front, 0, cells * sizeof *front);
    memset(back,  0, cells * sizeof *back);

    /* Clear screen, hide cursor. */
    console_write("\x1b[2J\x1b[H\x1b[?25l", 13);
    return true;
}

void tui_end(void) {
    /* background-SGR emission */
    console_write("\x1b[?25h\n", 7);
    kfree(front);
    kfree(back);
    front = back = NULL;
}

void tui_size(int *w, int *h) {
    if (w) *w = tui_w;
    if (h) *h = tui_h;
}

static inline bool in_bounds(int x, int y) {
    return x >= 0 && x < tui_w && y >= 0 && y < tui_h;
}

static inline struct cell *cell_at(struct cell *grid, int x, int y) {
    return &grid[(size_t)y * (size_t)tui_w + (size_t)x];
}

void tui_clear(uint8_t attr) {
    if (!back)
        return;
    size_t cells = (size_t)tui_w * (size_t)tui_h;
    for (size_t i = 0; i < cells; i++) {
        back[i].ch = ' ';
        back[i].attr = attr;
    }
}

void tui_put(int x, int y, char ch, uint8_t attr) {
    if (!back || !in_bounds(x, y))
        return;
    struct cell *c = cell_at(back, x, y);
    c->ch = ch;
    c->attr = attr;
}

void tui_text(int x, int y, uint8_t attr, const char *fmt, ...) {
    if (!back || y < 0 || y >= tui_h)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof buf - 1)
        n = (int)sizeof buf - 1;

    for (int i = 0; i < n; i++) {
        int xx = x + i;
        if (xx >= tui_w)
            break;
        if (xx < 0)
            continue;
        tui_put(xx, y, buf[i], attr);
    }
}

void tui_fill(struct tui_rect r, char ch, uint8_t attr) {
    if (!back)
        return;
    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            tui_put(x, y, ch, attr);
        }
    }
}

void tui_box(struct tui_rect r, uint8_t attr) {
    if (r.w < 2 || r.h < 2)
        return;
    int x0 = r.x, y0 = r.y;
    int x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;

    tui_put(x0, y0, '+', attr);
    tui_put(x1, y0, '+', attr);
    tui_put(x0, y1, '+', attr);
    tui_put(x1, y1, '+', attr);

    for (int x = x0 + 1; x < x1; x++) {
        tui_put(x, y0, '-', attr);
        tui_put(x, y1, '-', attr);
    }
    for (int y = y0 + 1; y < y1; y++) {
        tui_put(x0, y, '|', attr);
        tui_put(x1, y, '|', attr);
    }
}

/* Emit the diff. The output is a single console_write so the serial line
 * sees one contiguous burst per frame rather than one call per cell. */

// unused
//static void emit_attr(uint8_t attr, char *out, size_t *len) {
    // Only emit an SGR when the attribute changed from "default".
    // Simplest useful mapping: 0..7 -> 30..37, 8..15 -> 90..97,
    // and background 0..7 -> 40..47. Two sequences: reset, then set.
    //(void)attr; (void)out; (void)len;
//}

void tui_flush(void) {
    if (!front || !back)
        return;

    /* The output buffer: every cell that changed needs at most;
     * a cursor move (~8 bytes) + SGR reset + fg + bg (~15 bytes) + one character.
     * Budget 32 bytes per cell, cap the whole flush at a size that fits on the stack. */
    char out[8192];
    size_t len = 0;

    int last_x = -1, last_y = -1;
    uint8_t last_attr = 0xFF;

    for (int y = 0; y < tui_h; y++) {
        for (int x = 0; x < tui_w; x++) {
            struct cell *f = cell_at(front, x, y);
            struct cell *b = cell_at(back, x, y);
            if (f->ch == b->ch && f->attr == b->attr)
                continue;

            /* If the cursor is not already where we want it, move it. */
            if (x != last_x || y != last_y) {
                char seq[16];
                int n = ksnprintf(seq, sizeof seq, "\x1b[%d;%dH", y + 1, x + 1);
                for (int i = 0; i < n && len < sizeof out; i++)
                    out[len++] = seq[i];
                last_x = x;
                last_y = y;
            }
            /* If the attribute is not what it was, reset and set. */
            if (b->attr != last_attr) {
                const char *sgr;
                if (b->attr == 0) {
                    sgr = "\x1b[0m";
                } else {
                    /* Foreground only, for now. A full fg/bg table can come later. */
                    static const char *fg[8] = {
                        "\x1b[30m", "\x1b[31m", "\x1b[32m", "\x1b[33m",
                        "\x1b[34m", "\x1b[35m", "\x1b[36m", "\x1b[37m",
                    };
                    uint8_t a = b->attr & 0x0F;
                    sgr = (a < 8) ? fg[a] : "\x1b[0m";
                }
                size_t sl = strlen(sgr);
                for (size_t i = 0; i < sl && len < sizeof out; i++)
                    out[len++] = sgr[i];
                last_attr = b->attr;
            }
            if (len < sizeof out)
                out[len++] = b->ch;
            last_x = x + 1;

            *f = *b;
        }
    }

    if (len)
        console_write(out, len);
}

void tui_run(struct tui_view *v) {
    if (!v)
        return;

    /* Draw and flush once before the first key. */
    if (v->draw)
        v->draw(v);
    tui_flush();

    for (;;) {
        char c = 0;
        enum kbd_event ev = kbd_wait(&c);

        if (v->key && !v->key(v, ev, c))
            return;

        if (v->draw)
            v->draw(v);
        tui_flush();
    }
}
