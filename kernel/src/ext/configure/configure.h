#ifndef EXT_CONFIGURE_H
#define EXT_CONFIGURE_H

/* Run every registered probe, in order, called once during boot,
 * after the heap and VFS are up and before any app extension that might consult the host state.
 *
 * Each probe is a separate function that fills its section of `host` completely on success or not at all on failure.
 * Ordering matters only where one probe depends on another; today none do. */
void configure_run(void);

#endif
