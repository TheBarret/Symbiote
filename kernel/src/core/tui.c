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

/* Escape sequences used to be written inline with hand-counted lengths (13, 7).
 * Both were correct, but a future edit to the string would not update the count.
 * They are now file-scope constants passed with sizeof - 1. */
static const char tui_enter[] = "\x1b[0m\x1b[2J\x1b[H\x1b[?25l";
static const char tui_leave[] = "\x1b[0m\x1b[2J\x1b[H\x1b[?25h";

/* Patch note (2026-10): the SGR tables moved out of the per-cell loop.
 * They were function-local statics, which the compiler hoists,
 * but reading them as if they were declared per cell made the flush harder to follow than it needed to be.
 * Background is now emitted too; see the attribute comment in tui.h. */
static const char *const sgr_fg[8] = {
    "\x1b[30m", "\x1b[31m", "\x1b[32m", "\x1b[33m",
    "\x1b[34m", "\x1b[35m", "\x1b[36m", "\x1b[37m",
};
static const char *const sgr_bg[8] = {
    "\x1b[40m", "\x1b[41m", "\x1b[42m", "\x1b[43m",
    "\x1b[44m", "\x1b[45m", "\x1b[46m", "\x1b[47m",
};

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

    /* Reset SGR, clear screen, home, hide cursor. */
    console_write(tui_enter, sizeof tui_enter - 1);
    return true;
}

/* Previous tui_end only showed the cursor and wrote a newline.
 * That happened to look right because tui_begin had* cleared the screen and nothing had overwritten it,
 * but it left three things undone:
 *   - SGR was not reset, so the shell prompt inherited whatever color
 *     the last frame ended on;
 *   - the screen was not cleared, so a view that exited without a full
 *     redraw left its last cells on screen;
 *   - the cursor was not homed, so the prompt started on line 2.
 * The newline was a stand-in for the missing home. All three are now done explicitly, matching tui_begin's reset-and-clear. */
void tui_end(void) {
    console_write(tui_leave, sizeof tui_leave - 1);
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

/* The flush used to accumulate the whole frame* into a stack buffer before one console_write.
 * That kept the "one write per frame" property but introduced a silent failure mode:
 * when the buffer filled, the remaining changed cells were dropped and the front grid was not advanced past them,
 * so they never repainted.
 *
 * The property worth keeping is "the terminal sees a few large writes, not one per cell".
 * That is preserved here by draining the accumulator to console_write whenever it approaches full.
 * The drain re-arms the cursor and attribute trackers,
 * so the next cell re-emits a cursor move and an SGR and the terminal is left in a known state.
 *
 * The dead emit_attr helper below the flush was removed;
 * it had been superseded by the inline SGR selection and was kept only as a comment. */

#define TUI_FLUSH_BUF 4096
/* Leave room for the worst-case single-cell emission:
 * cursor move (8) + reset + fg + bg (12) + one char. 32 is generous. */
#define TUI_FLUSH_DRAIN_AT (TUI_FLUSH_BUF - 32)

void tui_flush(void) {
    if (!front || !back)
        return;

    char out[TUI_FLUSH_BUF];
    size_t len = 0;

    int last_x = -1, last_y = -1;
    uint8_t last_attr = 0xFF;

    for (int y = 0; y < tui_h; y++) {
        for (int x = 0; x < tui_w; x++) {
            struct cell *f = cell_at(front, x, y);
            struct cell *b = cell_at(back, x, y);
            if (f->ch == b->ch && f->attr == b->attr)
                continue;

            /* Drain before the worst-case emission can overflow. */
            if (len >= TUI_FLUSH_DRAIN_AT) {
                console_write(out, len);
                len = 0;
                /* The terminal is now somewhere we no longer track.
                 * Force the next cell to re-emit a cursor move and SGR. */
                last_x = -1;
                last_y = -1;
                last_attr = 0xFF;
            }

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
                uint8_t fg = b->attr & 0x0F;
                uint8_t bg = (b->attr >> 4) & 0x0F;
                size_t sl;

                /* Always reset first so a stale background from the
                 * previous cell does not leak into this one. */
                const char *reset = "\x1b[0m";
                sl = strlen(reset);
                for (size_t i = 0; i < sl && len < sizeof out; i++)
                    out[len++] = reset[i];

                if (b->attr != 0) {
                    if (fg < 8) {
                        const char *s = sgr_fg[fg];
                        sl = strlen(s);
                        for (size_t i = 0; i < sl && len < sizeof out; i++)
                            out[len++] = s[i];
                    }
                    if (bg != 0 && bg < 8) {
                        const char *s = sgr_bg[bg];
                        sl = strlen(s);
                        for (size_t i = 0; i < sl && len < sizeof out; i++)
                            out[len++] = s[i];
                    }
                }
                last_attr = b->attr;
            }

            if (len < sizeof out)
                out[len++] = b->ch;

            /* Last_y is now advanced alongside last_x.
             * Writing a character moves the cursor one column
             * to the right on the same row, so last_y must stay at y.
             * It happened to be correct before because last_y was only ever set inside the cursor-move branch,
             * but that is a coincidence, not an invariant. */
            last_x = x + 1;
            last_y = y;

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

/* Tui_run does not call tui_begin or tui_end, so every caller had to remember the pairing.
 * A view that returned through the panic path left the cursor hidden and the screen in TUI mode.
 * This entry point makes the pairing automatic. */
bool tui_run_view(struct tui_view *v) {
    if (!tui_begin())
        return false;
    tui_run(v);
    tui_end();
    return true;
}
