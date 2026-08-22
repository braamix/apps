/*
 * DureMark Porting Layer Header
 * Braam-specific abstractions for timing and I/O
 */
#include "proc/io.h" /* Task, write_all, proc_now */

/* Timing abstractions */
void du_start_time(void);

/* du_ticks_t is defined in duremark.h */
du_ticks_t du_get_time(void);

/* Get duration of one tick in seconds */
double du_get_sec_per_tick(void);

/* I/O abstractions. There is no printf here, and a write is a syscall a
   coroutine has to await, so du_printf builds text and du_flush sends it. */
void du_printf(const char *fmt, ...);
Task<void> du_flush(void);

/* What main.cpp defines: a coroutine, because it flushes. */
Task<i32> du_main(void);
