/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* ===  Dependencies  === */

#include "timefn.h"

#include "platform.h"

/*-****************************************
 *  Time functions
 ******************************************/

/* Braam has neither clock_gettime nor C90's clock(): proc_now() is the one
 * clock a process has, in milliseconds since boot. The harness freezes it, so
 * every span measured under the tests is zero. */

UTIL_time_t UTIL_getTime(void)
{
    UTIL_time_t r;
    r.t = (PTime)proc_now() * 1000000ULL;
    return r;
}

#define TIME_MT_MEASUREMENTS_NOT_SUPPORTED

/* ==== Common functions, valid for all time API ==== */

PTime UTIL_getSpanTimeNano(UTIL_time_t clockStart, UTIL_time_t clockEnd)
{
    return clockEnd.t - clockStart.t;
}

PTime UTIL_getSpanTimeMicro(UTIL_time_t begin, UTIL_time_t end)
{
    return UTIL_getSpanTimeNano(begin, end) / 1000ULL;
}

PTime UTIL_clockSpanMicro(UTIL_time_t clockStart)
{
    UTIL_time_t const clockEnd = UTIL_getTime();
    return UTIL_getSpanTimeMicro(clockStart, clockEnd);
}

PTime UTIL_clockSpanNano(UTIL_time_t clockStart)
{
    UTIL_time_t const clockEnd = UTIL_getTime();
    return UTIL_getSpanTimeNano(clockStart, clockEnd);
}

int UTIL_support_MT_measurements(void)
{
    return 0;
}
