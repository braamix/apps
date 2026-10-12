//
// Word-oriented buffered I/O interface.
// This implementation uses a buffer to handle word-sized I/O operations,
// built on top of standard file descriptors for underlying I/O.
// A `WFILE` structure includes a buffer, file descriptor, and state flags,
// with functions mirroring `stdio.h` but operating on `size_t` words in native byte order.
//
#include "wio.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xalloc.h"

#define BUFFER_SIZE (4096 / sizeof(size_t)) /* Buffer holds words, aligned to page size */

int wio_debug; // Enable manually for debug

//
// Open a file with appropriate flags based on mode (`r`, `w`, `a`),
// allocate a `WFILE` structure and buffer.
//
int wopen(WFILE *stream, const char *path, const char *mode)
{
    if (!stream) {
        return -1;
    }
    int flags = 0;
    char m    = mode[0];
    if (m == 'r') {
        flags = O_RDONLY;
    } else if (m == 'w') {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (m == 'a') {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        errno = EINVAL;
        return -1;
    }

    int fd = open(path, flags, 0666);
    if (fd == -1)
        return -1;

    stream->buffer = malloc(BUFFER_SIZE * sizeof(size_t));
    if (!stream->buffer) {
        close(fd);
        return -1;
    }

    stream->fd            = fd;
    stream->buffer_pos    = 0;
    stream->buffer_count  = 0;
    stream->is_eof        = false;
    stream->is_error      = false;
    stream->mode          = m;
    stream->must_close_fd = true;
    return 0;
}

//
// Close the existing stream (flushing if needed), then reopen with new parameters.
//
int wreopen(WFILE *stream, const char *path, const char *mode)
{
    if (!stream) {
        return -1;
    }
    if (stream->mode == 'w' || stream->mode == 'a') {
        wflush(stream);
    }
    if (stream->must_close_fd) {
        close(stream->fd);
        stream->must_close_fd = false;
    }
    free(stream->buffer);

    return wopen(stream, path, mode);
}

//
// Create a `WFILE` from an existing file descriptor.
//
int wdopen(WFILE *stream, int fildes, const char *mode)
{
    if (!stream) {
        return -1;
    }
    char m = mode[0];
    if (m != 'r' && m != 'w' && m != 'a') {
        errno = EINVAL;
        return -1;
    }

    stream->buffer = malloc(BUFFER_SIZE * sizeof(size_t));
    if (!stream->buffer) {
        return -1;
    }

    stream->fd            = fildes;
    stream->buffer_pos    = 0;
    stream->buffer_count  = 0;
    stream->is_eof        = false;
    stream->is_error      = false;
    stream->mode          = m;
    stream->must_close_fd = false;
    return 0;
}

//
// Write buffered data to the file for write/append modes.
//
int wflush(WFILE *stream)
{
    if (!stream || stream->mode == 'r') {
        errno = EINVAL;
        return -1;
    }

    if (stream->buffer_pos > 0) {
        ssize_t bytes_to_write = stream->buffer_pos * sizeof(size_t);
        ssize_t bytes_written  = write(stream->fd, stream->buffer, bytes_to_write);
        if (bytes_written != bytes_to_write) {
            stream->is_error = true;
            return -1;
        }
        stream->buffer_pos = 0;
    }
    return 0;
}

//
// Flush (if needed) and free resources.
//
void wclose(WFILE *stream)
{
    if (!stream)
        return;

    if (stream->mode == 'w' || stream->mode == 'a') {
        wflush(stream);
    }

    if (stream->must_close_fd) {
        close(stream->fd);
        stream->must_close_fd = false;
    }
    free(stream->buffer);
}

//
// Adjust file position in word units, flush buffer if writing.
//
int wseek(WFILE *stream, long offset, int whence)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }

    if (stream->mode == 'w' || stream->mode == 'a') {
        wflush(stream);
    }

    off_t new_offset = lseek(stream->fd, offset * sizeof(size_t), whence);
    if (new_offset == -1) {
        stream->is_error = true;
        return -1;
    }

    stream->buffer_pos   = 0;
    stream->buffer_count = 0;
    stream->is_eof       = false;
    return 0;
}

//
// Return current position in words, accounting for buffered data.
//
long wtell(WFILE *stream)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }

    off_t pos = lseek(stream->fd, 0, SEEK_CUR);
    if (pos == -1) {
        stream->is_error = true;
        return -1;
    }

    if (stream->mode == 'r') {
        pos -= stream->buffer_count * sizeof(size_t);
        pos += stream->buffer_pos * sizeof(size_t);
    } else {
        pos += stream->buffer_pos * sizeof(size_t);
    }

    return pos / sizeof(size_t);
}

//
// Seek to the start of the file.
//
void wrewind(WFILE *stream)
{
    if (stream) {
        wseek(stream, 0, SEEK_SET);
    }
}

//
// Read the next word, refilling the buffer if empty. Return `(size_t)-1` on EOF or error.
//
size_t wgetw(WFILE *stream)
{
    if (!stream || stream->mode != 'r') {
        errno = EINVAL;
        return (size_t)-1;
    }

    if (stream->buffer_pos >= stream->buffer_count) {
        stream->buffer_pos   = 0;
        stream->buffer_count = 0;

        ssize_t bytes_read = read(stream->fd, stream->buffer, BUFFER_SIZE * sizeof(size_t));
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                stream->is_eof = true;
            } else {
                stream->is_error = true;
            }
            return (size_t)-1;
        }

        stream->buffer_count = bytes_read / sizeof(size_t);
        if (bytes_read % sizeof(size_t) != 0) {
            stream->is_error = true;
            return (size_t)-1;
        }
    }

    size_t w = stream->buffer[stream->buffer_pos++];
    if (wio_debug) {
        printf("    %s %#zx\n", __func__, w);
    }
    return w;
}

//
// Buffer a word, flushing when the buffer is full.
//
int wputw(size_t w, WFILE *stream)
{
    if (!stream || (stream->mode != 'w' && stream->mode != 'a')) {
        errno = EINVAL;
        return -1;
    }

    if (stream->buffer_pos >= BUFFER_SIZE) {
        if (wflush(stream) != 0) {
            return -1;
        }
    }

    if (wio_debug) {
        printf("    %s %#zx\n", __func__, w);
    }
    stream->buffer[stream->buffer_pos++] = w;
    return 0;
}

//
// Check EOF state.
//
bool weof(const WFILE *stream)
{
    if (!stream)
        return true;
    return stream->is_eof;
}

//
// Check error state.
//
bool werror(const WFILE *stream)
{
    if (!stream)
        return true;
    return stream->is_error;
}

//
// Return the underlying file descriptor.
//
int wfileno(const WFILE *stream)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }
    return stream->fd;
}

//
// Clear EOF and error flags.
//
void wclearerr(WFILE *stream)
{
    if (stream) {
        stream->is_eof   = false;
        stream->is_error = false;
    }
}

//
// Read a zero terminated string, aligned to word boundary.
// Return a dynamically allocated buffer; the caller frees it with xfree().
// The buffer grows as needed, so the string may be of any length.
// In case of zero length string return NULL.
//
char *wgetstr(WFILE *stream)
{
    size_t capacity = 128; // words
    size_t *buf     = xalloc(capacity * sizeof(size_t), __func__, __FILE__, __LINE__);
    size_t n        = 0;

    for (;;) {
        buf[n] = wgetw(stream);
        if (stream->is_eof) {
            xfree(buf);
            return NULL;
        }
        if (n == 0 && buf[n] == 0) {
            // Read empty string.
            xfree(buf);
            return NULL;
        }
        // Does this word contain '\0' byte?
        bool is_last_word = memchr(&buf[n], '\0', sizeof(size_t)) != NULL;
        if (is_last_word) {
            if (wio_debug) {
                printf("    %s '%s'\n", __func__, (char *)buf);
            }
            char *str = xstrdup((char *)buf);
            xfree(buf);
            return str;
        }
        if (++n == capacity) {
            // Grow the word buffer: xalloc has no realloc, so copy into a larger block.
            size_t *bigger = xalloc(capacity * 2 * sizeof(size_t), __func__, __FILE__, __LINE__);
            memcpy(bigger, buf, capacity * sizeof(size_t));
            xfree(buf);
            buf = bigger;
            capacity *= 2;
        }
    }
}

//
// Write a zero terminated string, aligned to word boundary.
//
int wputstr(const char *str, WFILE *stream)
{
    if (wio_debug) {
        printf("    %s '%s'\n", __func__, str ? str : "(empty)");
    }
    if (str == NULL) {
        // Write empty string.
        return wputw(0, stream);
    }
    for (;;) {
        size_t w          = 0;
        bool is_last_word = memccpy(&w, str, '\0', sizeof(w)) != NULL;

        if (wputw(w, stream) < 0) {
            return -1;
        }
        if (is_last_word) {
            return 0;
        }
        str += sizeof(size_t);
    }
}

//
// Write a byte blob of a known length, aligned to word boundary: one word holding the
// byte count, then the bytes themselves, zero-padded to the last word.  Unlike wputstr()
// nothing terminates the data, so it may contain NUL bytes — which is what a decoded C
// string literal such as "a\0c" needs.
//
int wputdata(const void *data, size_t len, WFILE *stream)
{
    if (wio_debug) {
        printf("    %s %zu bytes\n", __func__, len);
    }
    if (wputw(len, stream) < 0) {
        return -1;
    }
    const char *p = data;
    for (size_t pos = 0; pos < len; pos += sizeof(size_t)) {
        size_t w     = 0;
        size_t chunk = len - pos;
        if (chunk > sizeof(size_t)) {
            chunk = sizeof(size_t);
        }
        memcpy(&w, p + pos, chunk);
        if (wputw(w, stream) < 0) {
            return -1;
        }
    }
    return 0;
}

//
// Read a byte blob written by wputdata().  Returns a dynamically allocated buffer of
// *len bytes (with one extra NUL appended, so it is safe to print when it holds text),
// or NULL for a zero-length blob or on EOF.  The caller frees it with xfree().
//
void *wgetdata(size_t *len, WFILE *stream)
{
    *len     = 0;
    size_t n = wgetw(stream);
    if (stream->is_eof || n == 0) {
        return NULL;
    }
    size_t nwords = (n + sizeof(size_t) - 1) / sizeof(size_t);
    char *buf     = xalloc(nwords * sizeof(size_t) + 1, __func__, __FILE__, __LINE__);
    for (size_t i = 0; i < nwords; i++) {
        size_t w = wgetw(stream);
        if (stream->is_eof) {
            xfree(buf);
            return NULL;
        }
        memcpy(buf + i * sizeof(size_t), &w, sizeof(size_t));
    }
    buf[n] = '\0';
    if (wio_debug) {
        printf("    %s %zu bytes\n", __func__, n);
    }
    *len = n;
    return buf;
}

//
// Read FP value. Return NaN on EOF or error.
//
double wgetd(WFILE *stream)
{
    if (sizeof(double) == sizeof(size_t)) {
        // One word
        union {
            size_t w;
            double f;
        } u;
        u.w = wgetw(stream);
        if (u.w == (size_t)-1)
            return nan("");
        return u.f;
    }
    if (sizeof(double) == 2 * sizeof(size_t)) {
        // Two words
        union {
            size_t w[2];
            double f;
        } u;
        u.w[0] = wgetw(stream);
        if (u.w[0] == (size_t)-1)
            return nan("");
        u.w[1] = wgetw(stream);
        if (u.w[1] == (size_t)-1)
            return nan("");
        return u.f;
    }
    // Cannot deserialize
    errno = EINVAL;
    return -1;
}

//
// Buffer an FP value.
//
int wputd(double f, WFILE *stream)
{
    if (sizeof(double) == sizeof(size_t)) {
        // One word
        union {
            double f;
            size_t w;
        } u;
        u.f = f;
        return wputw(u.w, stream);
    }
    if (sizeof(double) == 2 * sizeof(size_t)) {
        // Two words
        union {
            double f;
            size_t w[2];
        } u;
        u.f = f;
        if (wputw(u.w[0], stream) < 0)
            return -1;
        return wputw(u.w[1], stream);
    }
    // Cannot serialize
    errno = EINVAL;
    return -1;
}

Float128 wgetf128(WFILE *stream)
{
    Float128 f;
    f.lo = (uint64_t)wgetw(stream);
    f.hi = (uint64_t)wgetw(stream);
    return f;
}

int wputf128(Float128 f, WFILE *stream)
{
    if (wputw((size_t)f.lo, stream) < 0)
        return -1;
    return wputw((size_t)f.hi, stream);
}
