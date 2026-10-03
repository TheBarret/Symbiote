#include <core/ext.h>
#include <core/kprintf.h>
#include <core/panic.h>

/* The smallest possible extension: an init function and one line that
 * registers it. Copy this file to start a new one. */
static int hello_init(void) {
    kprintf("      hello: the extension model works\n");

#ifdef SYM_TEST_PANIC
    /* Build with CPPFLAGS=-DSYM_TEST_PANIC to see the panic screen. */
    PANIC("deliberate test panic, value=%d", 42);
#endif
    return 0;
}

SYM_EXTENSION(hello, hello_init, EXT_PRIO_APPLET);
