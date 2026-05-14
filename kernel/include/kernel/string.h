#ifndef OXYGEN_KERNEL_STRING_H
#define OXYGEN_KERNEL_STRING_H

#include <stddef.h>

/* Freestanding kernel needs to provide these because GCC may emit calls
 * to them implicitly (struct copies, large initializations, etc.). */
void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

#endif
