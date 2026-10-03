#include <stdbool.h>
#include <stddef.h>
#include <core/console.h>

#define CONSOLE_MAX 4
#define LOG_SIZE    16384   /* boot log kept in RAM for late-attaching consoles */

static const struct console_ops *consoles[CONSOLE_MAX];
static size_t console_count;

/* Ring buffer holding the most recent LOG_SIZE bytes of output. */
static char log_buf[LOG_SIZE];
static size_t log_head;      /* index where the next byte will be written */
static bool log_wrapped;     /* true once the ring has gone all the way round */

void console_write(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        log_buf[log_head] = buf[i];
        if (++log_head == LOG_SIZE) {
            log_head = 0;
            log_wrapped = true;
        }
    }
    for (size_t i = 0; i < console_count; i++)
        consoles[i]->write(buf, len);
}

void console_register(const struct console_ops *ops) {
    if (console_count >= CONSOLE_MAX)
        return;
    consoles[console_count++] = ops;

    /* Replay history, oldest byte first. */
    if (log_wrapped)
        ops->write(log_buf + log_head, LOG_SIZE - log_head);
    ops->write(log_buf, log_head);
}
