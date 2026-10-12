//
// Word-oriented buffered I/O interface.
//
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "float128.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// WFILE structure: Contains a file descriptor (`fd`), a buffer
// for `size_t` words, buffer position and count, EOF and error flags,
// and mode (`r`, `w`, or `a`).
//
struct _wfile {
    int fd;              /* Underlying file descriptor */
    size_t *buffer;      /* Buffer for read/write operations */
    size_t buffer_pos;   /* Current position in buffer */
    size_t buffer_count; /* Number of words in buffer */
    bool is_eof;         /* End-of-file flag */
    bool is_error;       /* Error flag */
    bool must_close_fd;  /* after wopen */
    char mode;           /* 'r' for read, 'w' for write, 'a' for append */
};
typedef struct _wfile WFILE;

int wopen(WFILE *stream, const char *path, const char *mode);
int wreopen(WFILE *stream, const char *path, const char *mode);
int wdopen(WFILE *stream, int fildes, const char *mode);
void wclose(WFILE *stream);
int wflush(WFILE *stream);
int wseek(WFILE *stream, long offset, int whence);
long wtell(WFILE *stream);
void wrewind(WFILE *stream);
size_t wgetw(WFILE *stream);
int wputw(size_t w, WFILE *stream);
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
