#include <core/ext.h>
#include <core/kprintf.h>
#include <core/panic.h>

/* Template */

static int test_init(void) {
    kprintf("Test message: Hello, World!\n");
    return 0;
}

SYM_EXTENSION(test, test_init, EXT_PRIO_APPLET);
