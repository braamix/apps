// SPDX-License-Identifier: 0BSD
#pragma once

#define tuklib_attr_noreturn
#define lzma_attribute(x)
#define lzma_attr_alloc_size(x)

#ifndef assert
#define assert(x) ((void)0)
#endif

// clang-format off
#include "braam.h"
#include "getopt.h"

#include "main.h"
#include "mytime.h"
#include "coder.h"
#include "message.h"
#include "args.h"
#include "hardware.h"
#include "file_io.h"
#include "options.h"
#include "suffix.h"
#include "util.h"
#include "list.h"
// clang-format on

// tuklib shims (English only, no gettext).
static inline const char *tuklib_mask_nonprint(const char *s)
{
    return s != NULL ? s : "";
}

static inline const char *tuklib_mask_nonprint_r(const char *s, char **mem)
{
    (void)mem;
    return tuklib_mask_nonprint(s);
}

static inline size_t tuklib_mbstr_width(const char *s, size_t *bytes)
{
    size_t n = s != NULL ? strlen(s) : 0;
    if (bytes != NULL)
        *bytes = n;
    return n;
}

static inline int tuklib_mbstr_fw(const char *s, int columns)
{
    size_t len;
    size_t width = tuklib_mbstr_width(s, &len);
    if (width > (size_t)columns)
        return 0;
    if (width < (size_t)columns)
        len += (size_t)columns - width;
    return (int)len;
}

static inline void tuklib_progname_init(char **argv)
{
    (void)argv;
}

static inline void tuklib_gettext_init(const char *pkg, const char *dir)
{
    (void)pkg;
    (void)dir;
}

void tuklib_exit(int status, int err_status, bool show_msg);

static inline void signals_init(void)
{
}
static inline void signals_exit(void)
{
}
static inline void signals_block(void)
{
}
static inline void signals_unblock(void)
{
}
