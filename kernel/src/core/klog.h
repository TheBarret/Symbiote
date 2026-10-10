#ifndef CORE_KLOG_H
#define CORE_KLOG_H

#include "core/version.h"

// Moved to ref: core/version.h
//#define SYM_TAG "\x1b[1;91m[KERN]\x1b[0m"
//#define SYM_TAG_EXT "\x1b[1;96m[EXT]\x1b[0m"
//#define SYM_TAG_INFO "\x1b[1;94m[INFO]\x1b[0m"
//#define SYM_TAG_WARNING "\x1b[1;93m[WARNING]\x1b[0m"
//#define SYM_TAG_ERROR "\x1b[1;91m[ERROR]\x1b[0m"

/* klog */
void klog_init(void);
void klog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void klog_ext(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void klog_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void klog_warning(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void klog_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
