// The porting layer: a small libc, and stdio over proc/file.h.
//
// Each libc routine is extern "C" — the compiler emits calls under the C names
// — and built with -fno-builtin-<name>, or the definition becomes a call to
// itself. Replaces unix/unix.c and tailor.h's stream macros.
#pragma once

#include "kernel/types.h"

// EOF is zip.h's, which this header is included by; getc needs one of its own.
constexpr int EOF_ = -1;

typedef i64 time_t;
#include "proc/file.h"
#include "proc/io.h"

// ------------------------------------------------------------------- the libc

extern "C" {

void *malloc(usize n);
void *calloc(usize n, usize size);
void *realloc(void *p, usize n);
void free(void *p);

void *memcpy(void *d, const void *s, usize n);
void *memmove(void *d, const void *s, usize n);
void *memset(void *d, int c, usize n);
int memcmp(const void *a, const void *b, usize n);

usize strlen(const char *s);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, usize n);
char *strcat(char *d, const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, usize n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *h, const char *n);
char *strncat(char *d, const char *s, usize n);

int toupper(int c);
int tolower(int c);
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isprint(int c);
int iscntrl(int c);

// getenv over the environment the spawn handed this process. There is no
// setenv anywhere, so the answer never changes.
char *getenv(const char *name);

int atoi(const char *s);

// The only two sscanf formats upstream uses, both for -t and -tt: an ISO
// yyyy-mm-dd or an American mmddyyyy. A general sscanf would be the rest of
// stdio for two call sites.
int zparse_date(const char *s, int *yyyy, int *mm, int *dd);
long strtol(const char *s, char **end, int base);

void qsort(void *base, usize n, usize size, int (*cmp)(const void *, const void *));

} // extern "C"

// ------------------------------------------------------------------- stdio

// Upstream's FILE. File is stdio's shape: a buffer, a sticky error, a flush.
using FILE = File;

// Assigned at startup: a global may have no destructor, and File::stderr() is
// not a constant expression.
extern FILE *zstdout;
extern FILE *zstderr;

// Every formatted message is built here. A message carries a path and FNMAX is
// 1024, so this cannot be a Buf<N> in a frame — a frame past 512 bytes costs a
// whole 64 KiB span. Nothing runs between the format and its write.
constexpr usize ZFMT_MAX = 1024 + 4096;
extern char zfmtbuf[ZFMT_MAX];

// The conversions upstream uses: %s %c %d %u %ld %lu %lld %llu %x %o %%, with
// an optional field width, zero pad and `-`. Returns the length written.
usize zvformat(char *out, usize cap, const char *fmt, __builtin_va_list ap);

// ---------------------------------------------------------- the stream layer

// fwrite over any stream. Upstream's own zfwrite(b,s,c) is bfwrite to the
// archive, so this one carries the FILE and has another name.
struct Zwrite : FileWrite {
    usize items = 0;

    usize await_resume() const { return v.is_ok() ? items : 0; }
};

inline Zwrite zwrite(const void *buf, usize size, usize count, FILE *f)
{
    Zwrite w;
    w.f     = f;
    w.s     = Str((const char *)buf, size * count);
    w.items = count;
    return w;
}

// fread: whole items read. File::read answers with what is there rather than
// what was asked for, so this is stdio's loop and not one call.
Task<usize> zfread(void *buf, usize size, usize count, FILE *f);

// putc: one byte, never a rune — File::put() would encode UTF-8. The byte is a
// global so the Str outlives the call; the write is awaited before anything
// else runs.
extern char zputbuf;

struct Zfputc : FileWrite {
    int await_resume() const { return v.is_ok() ? (unsigned char)zputbuf : -1; }
};

inline Zfputc zfputc(int c, FILE *f)
{
    zputbuf = (char)c;
    Zfputc w;
    w.f = f;
    w.s = Str(&zputbuf, 1);
    return w;
}

// getc: one byte, not a rune — File::get() would decode UTF-8, and getnam
// reads a name a byte at a time. EOF at end of input.
extern char zgetbuf;

struct Zfgetc : FileRead {
    int await_resume() const { return v.is_ok() && v.value() == 1 ? (unsigned char)zgetbuf : EOF_; }
};

inline Zfgetc zfgetc(FILE *f)
{
    Zfgetc r;
    r.f    = f;
    r.into = Span<char>(&zgetbuf, 1);
    return r;
}

// puts() and putchar(): stdout, and puts adds the newline.
Task<int> zfputs_nl(const char *s);

inline Zfputc zfputc_out(int c)
{
    return zfputc(c, zstdout);
}

inline FileWrite zfputs(const char *s, FILE *f)
{
    return f->write(Str(s, strlen(s)));
}

// Formats into zfmtbuf, then one write.
FileWrite zfprintf(FILE *f, const char *fmt, ...);

// sprintf. Synchronous: it touches no stream.
int zsprintf(char *out, const char *fmt, ...);

Task<Result<void>> zfflush(FILE *f);
Task<Result<void>> zfclose(FILE *f);
Task<i32> zfseeko(FILE *f, i64 off, int whence);
Task<i64> zftello(FILE *f);

inline int zferror(FILE *f)
{
    return f->failed() && !f->eof();
}

inline int zfeof(FILE *f)
{
    return f->eof();
}

// stdio's whence values.
#define SEEK_SET SYS_SEEK_SET
#define SEEK_CUR SYS_SEEK_CUR
#define SEEK_END SYS_SEEK_END

// ------------------------------------------- what unix/zipup.h did: raw reads
//
// The entry being compressed wants no buffer of its own: deflate reads it a
// window at a time and the store path a SBSZ at a time, so a descriptor is
// enough and a File would only copy.

typedef i32 ftype;

constexpr ftype fbad   = -1;
constexpr ftype zstdin = 0;
#define fhow 0

Task<ftype> zopen(const char *n, int how);
Task<unsigned> zread(ftype f, char *buf, unsigned len);
Task<void> zclose(ftype f);

// Upstream read this as "the last zread returned -1". A short read is the end
// of input here and never an error, so the flag is the port's own.
extern int zread_failed;
#define zerr(f) (zread_failed)

// ---------------------------------------------------- what unix/unix.c did

constexpr u32 DOSTIME_MINIMUM_ = 0x00210000u; // 1980-01-01 00:00:00

// Filesystem mtime (ms since the epoch) to a packed DOS stamp. 0 — every
// directory, since OPFS keeps no timestamp on one — is the DOS epoch.
u32 zdostime(u64 mtime_ms);

// The inverse, in seconds. proc/time.h has civil() and no way back.
i64 zdos2unix(u32 dostime);

extern i32 ztz_min;     // minutes east of UTC, read once at startup
extern i64 zstart_time; // seconds since the epoch, likewise

Task<void> zclock_init();

// time(). The wall clock read at startup plus the monotonic milliseconds
// since; under the harness proc_now() never advances, so it stands still.
extern "C" time_t time(time_t *t);
