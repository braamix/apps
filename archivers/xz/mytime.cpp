// SPDX-License-Identifier: 0BSD
#include "private.h"

uint64_t opt_flush_timeout = 0;

static u64 start_time;
static u64 next_flush;

extern void mytime_set_start_time(void)
{
    start_time = proc_now();
}

extern uint64_t mytime_get_elapsed(void)
{
    u64 now = proc_now();
    return now >= start_time ? now - start_time : 0;
}

extern void mytime_set_flush_time(void)
{
    if (opt_flush_timeout != 0)
        next_flush = proc_now() + opt_flush_timeout;
}

extern int mytime_get_flush_timeout(void)
{
    if (opt_flush_timeout == 0)
        return -1;
    u64 now = proc_now();
    if (now >= next_flush)
        return 0;
    u64 left = next_flush - now;
    if (left > INT_MAX)
        left = INT_MAX;
    return (int)left;
}
