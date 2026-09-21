// SPDX-License-Identifier: 0BSD — Braam port of file_io.c (simplified).
#include <fcntl.h>

#include "private.h"

extern const char stdin_filename[];

static bool try_sparse = true;

extern void io_init(void)
{
}

extern void io_no_sparse(void)
{
    try_sparse = false;
    (void)try_sparse;
}

static Task<bool> open_src_real(file_pair *pair)
{
    if (pair->src_name == stdin_filename) {
        pair->src_fd = STDIN_FILENO;
        co_return false;
    }

    int fd = co_await b_open(pair->src_name, SYS_O_READ, 0);
    if (fd < 0) {
        message_error(_("%s: %s"), tuklib_mask_nonprint(pair->src_name), strerror(errno));
        co_return true;
    }
    pair->src_fd = fd;

    if ((co_await b_fstat(fd, &pair->src_st)) != 0) {
        message_error(_("%s: %s"), tuklib_mask_nonprint(pair->src_name), strerror(errno));
        co_return true;
    }

    if (S_ISDIR(pair->src_st.st_mode)) {
        message_warning(_("%s: Is a directory, skipping"), tuklib_mask_nonprint(pair->src_name));
        co_return true;
    }

    if (!opt_stdout && !S_ISREG(pair->src_st.st_mode)) {
        message_warning(_("%s: Not a regular file, skipping"),
                        tuklib_mask_nonprint(pair->src_name));
        co_return true;
    }

    co_return false;
}

extern Task<file_pair *> io_open_src(const char *src_name)
{
    if (src_name[0] == '\0') {
        message_error(_("Empty filename, skipping"));
        co_return NULL;
    }

    static file_pair pair;
    pair = (file_pair){
        .src_name            = src_name,
        .dest_name           = NULL,
        .src_fd              = -1,
        .dest_fd             = -1,
        .src_eof             = false,
        .src_has_seen_input  = false,
        .flush_needed        = false,
        .dest_try_sparse     = false,
        .dest_pending_sparse = 0,
    };

    if (co_await open_src_real(&pair))
        co_return NULL;
    co_return &pair;
}

static Task<bool> open_dest_real(file_pair *pair)
{
    if (opt_stdout || pair->src_fd == STDIN_FILENO) {
        pair->dest_name = (char *)"(stdout)";
        pair->dest_fd   = STDOUT_FILENO;
        co_return false;
    }

    pair->dest_name = suffix_get_dest_name(pair->src_name);
    if (pair->dest_name == NULL)
        co_return true;

    if (!opt_force) {
        struct stat st;
        if ((co_await b_stat(pair->dest_name, &st)) == 0) {
            message_error(_("%s: File exists"), tuklib_mask_nonprint(pair->dest_name));
            free(pair->dest_name);
            pair->dest_name = NULL;
            co_return true;
        }
    }

    int flags = SYS_O_WRITE | SYS_O_CREATE | SYS_O_EXCL;
    int fd    = co_await b_open(pair->dest_name, flags, 0600);
    if (fd < 0) {
        message_error(_("%s: %s"), tuklib_mask_nonprint(pair->dest_name), strerror(errno));
        free(pair->dest_name);
        pair->dest_name = NULL;
        co_return true;
    }

    pair->dest_fd = fd;
    xz_remove_out = pair->dest_name;
    co_return false;
}

extern Task<bool> io_open_dest(file_pair *pair)
{
    co_return co_await open_dest_real(pair);
}

extern Task<size_t> io_read(file_pair *pair, io_buf *buf, size_t size)
{
    assert(size <= IO_BUFFER_SIZE);
    ssize_t amount = co_await xz_read_retry(pair->src_fd, buf->u8, size);
    if (amount < 0) {
        if (user_abort)
            co_return SIZE_MAX;
        message_error(_("%s: Read error: %s"), tuklib_mask_nonprint(pair->src_name),
                      strerror(errno));
        co_return SIZE_MAX;
    }
    if (amount == 0) {
        pair->src_eof = true;
        co_return 0;
    }
    pair->src_has_seen_input = true;
    co_return (size_t) amount;
}

extern Task<bool> io_write(file_pair *pair, const io_buf *buf, size_t size)
{
    assert(size <= IO_BUFFER_SIZE);
    if (pair->dest_fd == STDOUT_FILENO && opt_mode == MODE_COMPRESS && is_tty_stdout())
        co_return true;

    ssize_t w = co_await xz_write_retry(pair->dest_fd, buf->u8, size);
    if (w < 0 || (size_t)w != size) {
        message_error(_("%s: Write error: %s"),
                      pair->dest_name ? tuklib_mask_nonprint(pair->dest_name) : "?",
                      strerror(errno));
        co_return true;
    }
    co_return false;
}

extern Task<void> io_fix_src_pos(file_pair *pair, size_t rewind_size)
{
    if (rewind_size > 0)
        (void)co_await xz_lseek_retry(pair->src_fd, -(off_t)rewind_size, SEEK_CUR);
    co_return;
}

extern Task<bool> io_seek_src(file_pair *pair, uint64_t pos)
{
    off_t r = co_await xz_lseek_retry(pair->src_fd, (off_t)pos, SEEK_SET);
    if (r < 0) {
        message_error(_("%s: Seek error: %s"), tuklib_mask_nonprint(pair->src_name),
                      strerror(errno));
        co_return true;
    }
    pair->src_eof = false;
    co_return false;
}

extern Task<bool> io_pread(file_pair *pair, io_buf *buf, size_t size, uint64_t pos)
{
    if (co_await io_seek_src(pair, pos))
        co_return true;
    ssize_t amount = co_await xz_read_retry(pair->src_fd, buf->u8, size);
    if (amount < 0) {
        message_error(_("%s: Read error: %s"), tuklib_mask_nonprint(pair->src_name),
                      strerror(errno));
        co_return true;
    }
    if ((size_t)amount != size) {
        message_error(_("%s: Unexpected end of file"), tuklib_mask_nonprint(pair->src_name));
        co_return true;
    }
    co_return false;
}

static Task<void> close_dest(file_pair *pair, bool success)
{
    if (pair->dest_fd == -1 || pair->dest_fd == STDOUT_FILENO)
        co_return;

    if (co_await b_close(pair->dest_fd)) {
        message_error(_("%s: Closing the file failed: %s"), tuklib_mask_nonprint(pair->dest_name),
                      strerror(errno));
        success = false;
    }
    pair->dest_fd = -1;

    if (!success && pair->dest_name != NULL && strcmp(pair->dest_name, "(stdout)") != 0) {
        co_await b_unlink(pair->dest_name);
        xz_remove_out = NULL;
    }

    if (pair->dest_name != NULL && strcmp(pair->dest_name, "(stdout)") != 0) {
        free(pair->dest_name);
        pair->dest_name = NULL;
    }
}

static Task<void> close_src(file_pair *pair, bool success)
{
    if (pair->src_fd != STDIN_FILENO && pair->src_fd != -1) {
        co_await b_close(pair->src_fd);
        pair->src_fd = -1;
        if (success && !opt_keep_original)
            co_await b_unlink(pair->src_name);
    }
}

extern Task<void> io_close(file_pair *pair, bool success)
{
    co_await close_dest(pair, success);
    co_await close_src(pair, success);
    xz_remove_out = NULL;
    co_return;
}
