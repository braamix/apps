// SPDX-License-Identifier: 0BSD
#pragma once

#include "proc/io.h"

#if BUFSIZ <= 1024
#define IO_BUFFER_SIZE 8192
#else
#define IO_BUFFER_SIZE (BUFSIZ & ~7U)
#endif

typedef union {
    uint8_t u8[IO_BUFFER_SIZE];
    uint32_t u32[IO_BUFFER_SIZE / sizeof(uint32_t)];
    uint64_t u64[IO_BUFFER_SIZE / sizeof(uint64_t)];
} io_buf;

typedef struct {
    const char *src_name;
    char *dest_name;
    int src_fd;
    int dest_fd;
    bool src_eof;
    bool src_has_seen_input;
    bool flush_needed;
    bool dest_try_sparse;
    off_t dest_pending_sparse;
    struct stat src_st;
    struct stat dest_st;
} file_pair;

extern void io_init(void);
extern void io_no_sparse(void);

extern Task<file_pair *> io_open_src(const char *src_name);
extern Task<bool> io_open_dest(file_pair *pair);
extern Task<void> io_close(file_pair *pair, bool success);

extern Task<size_t> io_read(file_pair *pair, io_buf *buf, size_t size);
extern Task<void> io_fix_src_pos(file_pair *pair, size_t rewind_size);
extern Task<bool> io_seek_src(file_pair *pair, uint64_t pos);
extern Task<bool> io_pread(file_pair *pair, io_buf *buf, size_t size, uint64_t pos);
extern Task<bool> io_write(file_pair *pair, const io_buf *buf, size_t size);
