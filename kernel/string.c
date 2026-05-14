#include <stdint.h>
#include <stddef.h>

#include "kernel/string.h"

void *memset(void *dst, int c, size_t n) {
    uint8_t *p = dst;
    uint8_t  v = (uint8_t)c;
    for (size_t i = 0; i < n; i++) {
        p[i] = v;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t       *d = dst;
    const uint8_t *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t       *d = dst;
    const uint8_t *s = src;
    /* Overlapping forward copy would clobber its own source - copy
     * backwards when dst lies inside [src, src+n). */
    if (d > s && d < s + n) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a;
    const uint8_t *q = b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i]) {
            return (int)p[i] - (int)q[i];
        }
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}
