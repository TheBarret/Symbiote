#include <core/host.h>

/* The single instance, lives in .bss, zeroed at boot, so every field starts as "not present".
 * A probe that doesn't run leaves its fields exactly here. */
struct host_state host;
