#ifndef CORE_BOOT_H
#define CORE_BOOT_H

#include <stdbool.h>
#include <limine.h>

/* True if the bootloader speaks the protocol revision we asked for. */
bool boot_protocol_ok(void);

/* First framebuffer the bootloader gave us, or NULL (headless / serial-only). */
struct limine_framebuffer *boot_framebuffer(void);

#endif
