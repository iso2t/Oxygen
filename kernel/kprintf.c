#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "kernel/kprintf.h"
#include "kernel/vga.h"
#include "kernel/uart.h"
#include "kernel/spinlock.h"

static kspinlock_t kprintf_lock = KSPINLOCK_INIT;

static void kprint_putc(char c) {
    vga_putc(c);
    uart_putc(c);
}

static void emit_str(const char *s) {
    if (!s) {
        s = "(null)";
    }
    while (*s) {
        kprint_putc(*s++);
    }
}

static void emit_uint(uint64_t value, unsigned base, bool upper, int width, bool zero_pad) {
    char buf[32];
    int  len = 0;

    if (value == 0) {
        buf[len++] = '0';
    } else {
        const char *digits = upper
            ? "0123456789ABCDEF"
            : "0123456789abcdef";
        while (value > 0 && len < (int)sizeof(buf)) {
            buf[len++] = digits[value % base];
            value /= base;
        }
    }

    const char pad = zero_pad ? '0' : ' ';
    while (len < width && len < (int)sizeof(buf)) {
        buf[len++] = pad;
    }

    while (len-- > 0) {
        kprint_putc(buf[len]);
    }
}

static void emit_int(int64_t value, int width, bool zero_pad) {
    if (value < 0) {
        kprint_putc('-');
        if (width > 0) {
            width--;
        }
        emit_uint((uint64_t)(-value), 10, false, width, zero_pad);
    } else {
        emit_uint((uint64_t)value, 10, false, width, zero_pad);
    }
}

static void emit_ptr(const void *p) {
    kprint_putc('0');
    kprint_putc('x');
    emit_uint((uint64_t)(uintptr_t)p, 16, false, 16, true);
}

void kvprintf(const char *fmt, va_list ap) {
    while (*fmt) {
        if (*fmt != '%') {
            kprint_putc(*fmt++);
            continue;
        }

        fmt++;  /* eat '%' */

        bool zero_pad     = false;
        int  width        = 0;
        bool length_long  = false;

        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        if (*fmt == 'l') {
            length_long = true;
            fmt++;
            if (*fmt == 'l') {
                fmt++;  /* %ll == %l on x86_64 */
            }
        } else if (*fmt == 'z') {
            length_long = true;  /* size_t is 64-bit on x86_64 */
            fmt++;
        }

        char spec = *fmt;
        switch (spec) {
        case 'd':
        case 'i': {
            int64_t v = length_long
                ? va_arg(ap, int64_t)
                : (int64_t)va_arg(ap, int);
            emit_int(v, width, zero_pad);
            break;
        }
        case 'u': {
            uint64_t v = length_long
                ? va_arg(ap, uint64_t)
                : (uint64_t)va_arg(ap, unsigned int);
            emit_uint(v, 10, false, width, zero_pad);
            break;
        }
        case 'x': {
            uint64_t v = length_long
                ? va_arg(ap, uint64_t)
                : (uint64_t)va_arg(ap, unsigned int);
            emit_uint(v, 16, false, width, zero_pad);
            break;
        }
        case 'X': {
            uint64_t v = length_long
                ? va_arg(ap, uint64_t)
                : (uint64_t)va_arg(ap, unsigned int);
            emit_uint(v, 16, true, width, zero_pad);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            emit_str(s);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            kprint_putc(c);
            break;
        }
        case 'p': {
            const void *p = va_arg(ap, const void *);
            emit_ptr(p);
            break;
        }
        case '%':
            kprint_putc('%');
            break;
        case 0:
            return;  /* trailing '%' at end of format */
        default:
            /* Unknown spec - echo literal so the bug is visible. */
            kprint_putc('%');
            kprint_putc(spec);
            break;
        }

        if (*fmt) {
            fmt++;
        }
    }
}

void kprintf_unlocked(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void kprintf(const char *fmt, ...) {
    unsigned long flags = kspinlock_acquire(&kprintf_lock);
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kspinlock_release(&kprintf_lock, flags);
}
