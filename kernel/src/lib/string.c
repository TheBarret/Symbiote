#include <stddef.h>
#include <lib/string.h>

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n] != '\0')
        n++;
    return n;
}

/* Returns <0, 0, >0 like the standard. */
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
