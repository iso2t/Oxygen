/*
 * Oxygen kernel shell.
 *
 * Reads keystrokes from the PS/2 keyboard ring buffer, builds a line,
 * tokenizes on Enter, dispatches into a static command table. Runs as
 * a kernel thread; the main kthread idles after spawning the shell.
 */

#include <stdint.h>
#include <stddef.h>

#include "kernel/kshell.h"
#include "kernel/keyboard.h"
#include "kernel/kprintf.h"
#include "kernel/string.h"
#include "kernel/thread.h"
#include "kernel/heap.h"
#include "kernel/pmm.h"
#include "kernel/pit.h"
#include "kernel/vga.h"
#include "kernel/panic.h"
#include "kernel/userspace.h"

#define KSHELL_LINE_MAX 128
#define KSHELL_ARGS_MAX 16

static int next_counter_id = 1;

/* ---------- small parsers ---------- */

static int parse_int(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

static uint64_t parse_hex(const char *s) {
    uint64_t v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    while (*s) {
        v <<= 4;
        if      (*s >= '0' && *s <= '9') v |= (uint64_t)(*s - '0');
        else if (*s >= 'a' && *s <= 'f') v |= (uint64_t)(*s - 'a' + 10);
        else if (*s >= 'A' && *s <= 'F') v |= (uint64_t)(*s - 'A' + 10);
        else break;
        s++;
    }
    return v;
}

static int tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p) {
            *p++ = 0;
        }
    }
    return argc;
}

/* ---------- demo workload spawned by `spawn` ---------- */

static void counter_thread(void *arg) {
    int id = (int)(uintptr_t)arg;
    for (int i = 0; i < 5; i++) {
        kprintf("[t%d:%d]", id, i);
        uint64_t target = pit_ticks() + 50;
        while (pit_ticks() < target) {
            __asm__ volatile ("hlt");
        }
    }
    kprintf("[t%d:done]", id);
}

/* ---------- command implementations ---------- */

struct cmd {
    const char *name;
    int       (*fn)(int argc, char **argv);
    const char *help;
};

static const struct cmd commands[];   /* forward */

static int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("commands:\n");
    for (const struct cmd *c = commands; c->name; c++) {
        kprintf("  %-7s  %s\n", c->name, c->help);
    }
    return 0;
}

static int cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    thread_dump();
    return 0;
}

static int cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("pmm:  %zu/%zu frames free (%zu MiB total)\n",
            pmm_free_frames(), pmm_total_frames(),
            (pmm_total_frames() * PMM_FRAME_SIZE) / (1024UL * 1024UL));
    heap_dump();
    return 0;
}

static int cmd_tick(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t t = pit_ticks();
    kprintf("ticks: %lu  (%lu.%02lu s @ 100 Hz)\n",
            t, t / 100, t % 100);
    return 0;
}

static int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    vga_clear();
    return 0;
}

static int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    kprintf("\n");
    return 0;
}

static int cmd_spawn(int argc, char **argv) {
    int n = (argc > 1) ? parse_int(argv[1]) : 1;
    if (n <= 0) n = 1;
    if (n > 16) n = 16;
    for (int i = 0; i < n; i++) {
        int id = next_counter_id++;
        if (kthread_create(counter_thread,
                           (void *)(uintptr_t)id, "counter") < 0) {
            kprintf("spawn: OOM at t%d\n", id);
            return -1;
        }
    }
    kprintf("spawned %d counter thread%s\n", n, (n == 1) ? "" : "s");
    return 0;
}

static int cmd_peek(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: peek 0xADDR\n");
        return -1;
    }
    uintptr_t addr = (uintptr_t)parse_hex(argv[1]);
    /* Deliberate: if addr isn't mapped, this triggers #PF and the IDT
     * dump prints register state. Useful demo. */
    uint64_t val = *(volatile uint64_t *)addr;
    kprintf("[%p] = 0x%016lx\n", (void *)addr, val);
    return 0;
}

static int cmd_panic(int argc, char **argv) {
    (void)argc; (void)argv;
    panic("triggered by kshell");
}

static int cmd_user(int argc, char **argv) {
    (void)argc; (void)argv;
    int tid = kthread_create(userspace_run, NULL, "user");
    if (tid < 0) {
        kprintf("user: failed to spawn thread\n");
        return -1;
    }
    kprintf("user: spawned ring-3 thread tid=%d\n", tid);
    return 0;
}

static int cmd_halt(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("halting.\n");
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }

    return 0;   /* never reached */
}

static const struct cmd commands[] = {
    {"help",  cmd_help,  "list commands"},
    {"ps",    cmd_ps,    "list kernel threads"},
    {"mem",   cmd_mem,   "memory subsystem stats"},
    {"tick",  cmd_tick,  "PIT tick count + uptime"},
    {"clear", cmd_clear, "clear the screen"},
    {"echo",  cmd_echo,  "print arguments"},
    {"spawn", cmd_spawn, "spawn N counter threads (default 1)"},
    {"peek",  cmd_peek,  "read 8 bytes from a hex address"},
    {"user",  cmd_user,  "run the first user (ring-3) program"},
    {"panic", cmd_panic, "trigger panic (testing)"},
    {"halt",  cmd_halt,  "stop the system"},
    {NULL, NULL, NULL},
};

/* ---------- dispatch + main loop ---------- */

static void kshell_execute(char *line) {
    char *argv[KSHELL_ARGS_MAX];
    int argc = tokenize(line, argv, KSHELL_ARGS_MAX);
    if (argc == 0) {
        return;
    }
    for (const struct cmd *c = commands; c->name; c++) {
        if (strcmp(c->name, argv[0]) == 0) {
            c->fn(argc, argv);
            return;
        }
    }
    kprintf("unknown command: %s (try `help`)\n", argv[0]);
}

void kshell_run(void *arg) {
    (void)arg;
    char   line[KSHELL_LINE_MAX];
    size_t len = 0;

    kprintf("\noxygen> ");

    for (;;) {
        int c = keyboard_getc();
        if (c < 0) {
            __asm__ volatile ("hlt");
            continue;
        }

        if (c == '\n') {
            line[len] = 0;
            kprintf("\n");
            kshell_execute(line);
            len = 0;
            kprintf("oxygen> ");
            continue;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b \b");      /* erase last char on screen */
            }
            continue;
        }

        if (c >= 32 && c < 127 && len < KSHELL_LINE_MAX - 1) {
            line[len++] = (char)c;
            kprintf("%c", (char)c);
        }
    }
}
