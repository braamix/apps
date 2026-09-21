/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

/* Braam: upstream's whole platform detection collapses to one answer.
 * wasm32 is 32-bit, not POSIX, and has no console mode to set. */

#ifndef PLATFORM_H_MODULE
#define PLATFORM_H_MODULE

#include "braam.h"

#define PLATFORM_POSIX_VERSION 0

#define SET_BINARY_MODE(file)
#define SET_SPARSE_FILE_MODE(file)

/* A store file is not sparse: a hole would still be stored. */
#define ZSTD_SPARSE_DEFAULT 0

#define ZSTD_START_SYMBOLLIST_FRAME 0
#define ZSTD_SETPRIORITY_SUPPORT    0
#define ZSTD_NANOSLEEP_SUPPORT      0

#endif /* PLATFORM_H_MODULE */
