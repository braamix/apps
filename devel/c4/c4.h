// C4's compiler and VM, as a driver. Nothing here blocks: braam.cpp performs
// the opens, reads, closes and writes that a burst asks for.
#pragma once

enum {
    C4_TICK = 0, // the burst is up; drain output and park
    C4_OPEN,     // c4_sys.path, c4_sys.flags -> c4_a
    C4_READ,     // c4_sys.fd, c4_sys.buf, c4_sys.n -> c4_a
    C4_CLOSE,    // c4_sys.fd -> c4_a
    C4_EXIT,     // guest exit: c4_status, c4_cycle; output already formatted
    C4_STOP,     // compiler error or unknown instruction; c4_status
};

struct C4Sys {
    const char *path;
    char *buf;
    long long flags;
    long long fd;
    long long n;
};

extern char *c4_obuf;
extern int c4_olen;
extern C4Sys c4_sys;
extern long long c4_a;
extern long long c4_cycle;
extern long long c4_status;

extern long long src;   // -s: dump source and assembly, do not run
extern long long debug; // -d: trace each instruction

int c4_alloc(int poolsz);
void c4_kwinit();
void c4_set_source(char *text);
int c4_compile();
int c4_vmsetup(int argc, char **argv);
int c4_burst();
void c4_set_a(long long v);
void c4_clear_out();
