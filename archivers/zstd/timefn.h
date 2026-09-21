/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef TIME_FN_H_MODULE_287987
#define TIME_FN_H_MODULE_287987

/*-****************************************
 *  Types
 ******************************************/

#include <stdint.h>     /* uint64_t */
typedef uint64_t PTime; /* Precise Time */

/* UTIL_time_t contains a nanosecond time counter.
 * The absolute value is not meaningful.
 * It's only valid to compute the difference between 2 measurements. */
typedef struct {
    PTime t;
} UTIL_time_t;
#define UTIL_TIME_INITIALIZER { 0 }

/*-****************************************
 *  Time functions
 ******************************************/

UTIL_time_t UTIL_getTime(void);

/* tells if timefn will return correct time measurements
 * in presence of multi-threaded workload.
 * note : this is not the case if only C90 clock_t measurements are available */
int UTIL_support_MT_measurements(void);

PTime UTIL_getSpanTimeNano(UTIL_time_t clockStart, UTIL_time_t clockEnd);
PTime UTIL_clockSpanNano(UTIL_time_t clockStart);

PTime UTIL_getSpanTimeMicro(UTIL_time_t clockStart, UTIL_time_t clockEnd);
PTime UTIL_clockSpanMicro(UTIL_time_t clockStart);

#define SEC_TO_MICRO ((PTime)1000000) /* nb of microseconds in a second */

#endif /* TIME_FN_H_MODULE_287987 */
