#include <core/ext.h>
#include "configure.h"

/* Configure probe extension
 *
 * Priority set high: EXT_PRIO_CONSOLE after boot memory and the heap are up,
 * before any applet that might want to read `host`.
 * The probe set is fixed at compile time; adding a probe is adding a run_probe line. */

static int configure_ext_init(void) {
    configure_run();
    return 0;
}

SYM_EXTENSION(probe_toolkit, configure_ext_init, EXT_PRIO_CONSOLE);
