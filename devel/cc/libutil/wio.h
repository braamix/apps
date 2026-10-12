//
// Word-oriented buffered I/O interface.
//
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "float128.h"
#include "host.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// WFILE structure: Contains a file descriptor (`fd`), a buffer
// for `size_t` words, buffer position and count, EOF and error flags,
// and mode (`r`, `w`, or `a`).
//
// The whole file is in the buffer: a stream for reading reads it all when it is
// opened, and one for writing keeps every word until wclose() writes them out. So
// opening and closing are the only calls that wait, coroutines on Braam, and the
// rest are plain functions over memory.
//
struct _wfile {
    int fd;              // Underlying file descriptor
    size_t *buffer;      // Buffer for read/write operations
    size_t buffer_pos;   // Current position in buffer
    size_t buffer_count; // Number of words in buffer
    size_t buffer_cap;   // Words the buffer has room for
    bool is_eof;         // End-of-file flag
    bool is_error;       // Error flag
    bool partial;        // The file ended inside a word
    bool must_close_fd;  // after wopen
    char mode;           // 'r' for read, 'w' for write, 'a' for append
};
typedef struct _wfile WFILE;

coro(braam_call *) int wopen(WFILE *stream, const char *path, const char *mode);
coro(braam_call *) int wreopen(WFILE *stream, const char *path, const char *mode);
coro(braam_call *) int wdopen(WFILE *stream, int fildes, const char *mode);
coro(braam_call *) void wclose(WFILE *stream);
// A write stream with no file, its words only kept in memory; wmemfree frees them.
void wmemopen(WFILE *stream);
void wmemfree(WFILE *stream);
int wflush(WFILE *stream);
int wseek(WFILE *stream, long offset, int whence);
long wtell(WFILE *stream);
void wrewind(WFILE *stream);
size_t wgetw(WFILE *stream);
int wputw(size_t w, WFILE *stream);
// A 64-bit value: one word where size_t holds it, two (low, high) where it does not.
unsigned long long wgetll(WFILE *stream);
int wputll(unsigned long long v, WFILE *stream);
double wgetd(WFILE *stream);
int wputd(double w, WFILE *stream);
// A binary128 value, as two words: low, then high.
Float128 wgetf128(WFILE *stream);
int wputf128(Float128 f, WFILE *stream);
bool weof(const WFILE *stream);
bool werror(const WFILE *stream);
int wfileno(const WFILE *stream);
void wclearerr(WFILE *stream);
char *wgetstr(WFILE *stream); // dynamically allocated
int wputstr(const char *str, WFILE *stream);
void *wgetdata(size_t *len, WFILE *stream); // dynamically allocated
int wputdata(const void *data, size_t len, WFILE *stream);

#ifdef __cplusplus
}
#endif
