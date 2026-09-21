// SPDX-License-Identifier: 0BSD
#pragma once

#include <stdint.h>

extern uint64_t opt_flush_timeout;

extern void mytime_set_start_time(void);
extern uint64_t mytime_get_elapsed(void);
extern void mytime_set_flush_time(void);
extern int mytime_get_flush_timeout(void);
