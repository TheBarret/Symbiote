#include <core/ext.h>
#include <core/kprintf.h>
#include <core/panic.h>

/* Extension 'Hello, World!' Example Template */

static int hello_init(void) {
    kprintf("[hello] Hello, World!\n");
    return 0;
}

SYM_EXTENSION(hello, hello_init, EXT_PRIO_APPLET);
