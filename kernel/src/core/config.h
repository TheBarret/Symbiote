#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/* incorrect settings can result in unpredictable behaviors!  */

/* Serial settings
 * Ref: src/core/serial.c
 */
#define COM1 0x3F8

/* PMM settings
 * Ref: src/core/pmm.h
 */
#define PMM_PAGE_SIZE 4096

/* VMM settings
 * Ref: src/core/vmm.h
 */
#define VMM_PAGE_SIZE 4096

/* PIC settings
 * Ref: src/core/pic.h
 */
#define PIC_VECTOR_BASE 0x20

/* Timer settings
 * Ref: src/core/timer.h
 * Ref: src/core/timer.c
 */
#define TIMER_HZ 1000
#define PIT_FREQ  1193182ull        /* Hz, the PIT's input clock */
#define PIT_CH0   0x40
#define PIT_CMD   0x43

/* Shell settings
 * Ref: src/core/shell.c
 */
#define SHELL_LINE_MAX  128

/* Console settings
 * Ref: src/core/console.c
 */
#define CONSOLE_MAX 4
#define LOG_SIZE    16384   /* boot log kept in RAM for late-attaching consoles */

/* VFS settings
 * Ref: src/core/vfs.h
 */
#define VFS_PATH_MAX 128
#define VFS_NAME_MAX 32

/* Klog settings
 * Ref: src/core/klog.h
 */
#define SYM_TAG "\x1b[1;91m[KERN]\x1b[0m"
#define SYM_TAG_EXT "\x1b[1;96m[EXT]\x1b[0m"
#define SYM_TAG_INFO "\x1b[1;94m[INFO]\x1b[0m"
#define SYM_TAG_WARNING "\x1b[1;93m[WARNING]\x1b[0m"
#define SYM_TAG_ERROR "\x1b[1;91m[ERROR]\x1b[0m"

/* fbcon settings
 * Theme: phosphor green on near-black (0xRRGGBB)
 * Ref: src/ext/fbcon/fbcon.c
 */
static const uint32_t SYM_FBCON_BG = 0x000a0a0a;
static const uint32_t SYM_FBCON_FG = 0x0033ff66;

#endif
