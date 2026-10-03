#ifndef LIB_MEM_H
#define LIB_MEM_H

#include <stddef.h>

/* The compiler is allowed to emit calls to these four even if you never wrote one,
   so they MUST exist and MUST behave exactly like the C standard says. */
void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

/* Route normal uses through the builtins so the compiler can inline them;
 * mem.c holds the real implementations it falls back to. */
#define memcpy  __builtin_memcpy
#define memset  __builtin_memset
#define memmove __builtin_memmove
#define memcmp  __builtin_memcmp

#endif
