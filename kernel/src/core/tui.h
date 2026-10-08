#ifndef CORE_TUI_H
#define CORE_TUI_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <core/keyboard.h>

/* Micro Terminal UI core
 *
 * Two cell grids (front and back), a rect-based drawing API,
 * and a flush that emits only the cells that changed.
 * Extensions draw into the back grid;
 * tui_flush diffs it against the front grid and writes the diff to the console in one call.
 * Nothing here knows about windows, clipping, or widgets; a view is a draw callback and a key callback. */

struct tui_rect {
    int x, y, w, h;
};

/* Attribute byte: low nibble = foreground, high nibble = background.
 * 0 means "default", which the flush leaves to the terminal. */
#define TUI_FG_BLACK   0x0
#define TUI_FG_RED     0x1
#define TUI_FG_GREEN   0x2
#define TUI_FG_YELLOW  0x3
#define TUI_FG_BLUE    0x4
#define TUI_FG_MAGENTA 0x5
#define TUI_FG_CYAN    0x6
#define TUI_FG_WHITE   0x7
#define TUI_ATTR(fg, bg) ((uint8_t)((fg) | ((bg) << 4)))
#define TUI_ATTR_DEFAULT 0

/* Start / stop. tui_begin allocates the grids, clears the screen, hides the cursor.
 * tui_end restores the cursor, clears, and frees. */
bool tui_begin(void);
void tui_end(void);

/* Screen size, in cells. Fixed at 80x25 for now; the ops struct has no size query. A later get_size hook would replace this. */
void tui_size(int *w, int *h);

/* Drawing. All of these write into the back grid; nothing is emitted to the console until tui_flush.
 * Out-of-range coordinates are ignored. */
void tui_clear(uint8_t attr);
void tui_put(int x, int y, char ch, uint8_t attr);
void tui_text(int x, int y, uint8_t attr, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
void tui_fill(struct tui_rect r, char ch, uint8_t attr);
void tui_box(struct tui_rect r, uint8_t attr);

/* Emit the diff between back and front to the console in one write,
 * then copy back to front. Call once per frame, after drawing. */
void tui_flush(void);

/* A view: draw into the grid, handle a key. key returns false to quit. */
struct tui_view {
    void (*draw)(struct tui_view *v);
    bool (*key)(struct tui_view *v, enum kbd_event ev, char c);
    void *state;
};

void tui_run(struct tui_view *v);

#endif
