/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef ZSTD_FILEIO_COMMON_H
#define ZSTD_FILEIO_COMMON_H

#include "fileio_types.h"
#include "platform.h"
#include "timefn.h" /* UTIL_getTime, UTIL_clockSpanMicro */

/*-*************************************
 *  Macros
 ***************************************/
#define KB *(1 << 10)
#define MB *(1 << 20)
#define GB *(1U << 30)
#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#undef MIN /* in case it would be already defined */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

extern FIO_display_prefs_t g_display_prefs;

/* Braam: a diagnostic is written where the process next parks (braam.cpp), so
 * DISPLAY stays a plain call that a non-coroutine can make. */
#define DISPLAY_F(f, ...) zstd_display((f), __VA_ARGS__)
#define DISPLAYOUT(...)   DISPLAY_F(stdout, __VA_ARGS__)
#define DISPLAY(...)      DISPLAY_F(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)                     \
    {                                            \
        if (g_display_prefs.displayLevel >= l) { \
            DISPLAY(__VA_ARGS__);                \
        }                                        \
    }

extern UTIL_time_t g_displayClock;

#define REFRESH_RATE ((U64)(SEC_TO_MICRO / 6))
#define READY_FOR_UPDATE() \
    (UTIL_clockSpanMicro(g_displayClock) > REFRESH_RATE || g_display_prefs.displayLevel >= 4)
#define DELAY_NEXT_UPDATE()              \
    {                                    \
        g_displayClock = UTIL_getTime(); \
    }
#define DISPLAYUPDATE(l, ...)                                    \
    {                                                            \
        if (g_display_prefs.displayLevel >= l &&                 \
            (g_display_prefs.progressSetting != FIO_ps_never)) { \
            if (READY_FOR_UPDATE()) {                            \
                DELAY_NEXT_UPDATE();                             \
                DISPLAY(__VA_ARGS__);                            \
            }                                                    \
        }                                                        \
    }

#define SHOULD_DISPLAY_SUMMARY() \
    (g_display_prefs.displayLevel >= 2 || g_display_prefs.progressSetting == FIO_ps_always)
#define SHOULD_DISPLAY_PROGRESS() \
    (g_display_prefs.progressSetting != FIO_ps_never && SHOULD_DISPLAY_SUMMARY())
#define DISPLAY_PROGRESS(...)             \
    {                                     \
        if (SHOULD_DISPLAY_PROGRESS()) {  \
            DISPLAYLEVEL(1, __VA_ARGS__); \
        }                                 \
    }
#define DISPLAYUPDATE_PROGRESS(...)        \
    {                                      \
        if (SHOULD_DISPLAY_PROGRESS()) {   \
            DISPLAYUPDATE(1, __VA_ARGS__); \
        }                                  \
    }
#define DISPLAY_SUMMARY(...)              \
    {                                     \
        if (SHOULD_DISPLAY_SUMMARY()) {   \
            DISPLAYLEVEL(1, __VA_ARGS__); \
        }                                 \
    }

/* Upstream's EXM_THROW ended with exit(error). There is no exit() here and no
 * longjmp to stand in for it, so the message and the status are all this does:
 * the caller returns on the next line, and zstd_fatal() then refuses every
 * later read and write, which is what stops a loop that does not check. */
#define EXM_ERROR(error, ...)                                                     \
    {                                                                             \
        DISPLAYLEVEL(1, "zstd: ");                                                \
        DISPLAYLEVEL(5, "Error defined at %s, line %i : \n", __FILE__, __LINE__); \
        DISPLAYLEVEL(1, "error %i : ", error);                                    \
        DISPLAYLEVEL(1, __VA_ARGS__);                                             \
        DISPLAYLEVEL(1, " \n");                                                   \
        zstd_fail(error);                                                         \
    }

#define CHECK_V(v, f)                              \
    v = f;                                         \
    if (ZSTD_isError(v)) {                         \
        DISPLAYLEVEL(5, "%s \n", #f);              \
        EXM_ERROR(11, "%s", ZSTD_getErrorName(v)); \
    }
#define CHECK(f)         \
    {                    \
        size_t err;      \
        CHECK_V(err, f); \
    }

/* long is 32 bits here, so the pair that takes an off_t is the only one that
 * can reach past 2 GiB. */
#define LONG_SEEK b_fseeko
#define LONG_TELL b_ftello

#endif /* ZSTD_FILEIO_COMMON_H */
