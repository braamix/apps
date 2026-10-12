/*
 * The asynchronous system calls of Braam (docs/Braam.md §7.3): braam_sys, the
 * one primitive, and the descriptor, file and directory calls on it, each a coroutine
 * yielding braam_call *.
 */
#include <braam.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int errno;

void __braam_forget(braam_call *c);

coro(braam_call *) int braam_sys(unsigned op, const void *payload, unsigned len, void *out,
                                 unsigned cap, unsigned *got)
{
    braam_call c = { op, 0, payload, len, NULL, 0 };
    defer __braam_forget(&c); /* destroyed while waiting: the reply goes unclaimed */
    yield & c;
    int status = -8; /* Io: a reply too short for its status */
    unsigned n = 0;
    if (c.reply_len >= 4) {
        const unsigned char *r = c.reply;
        status                 = (int)(r[0] | r[1] << 8 | r[2] << 16 | (unsigned)r[3] << 24);
        n                      = c.reply_len - 4 < cap ? c.reply_len - 4 : cap;
        memcpy(out, r + 4, n);
    }
    free(c.reply);
    if (got)
        *got = n;
    return status;
}

unsigned braam_now(void)
{
    return (unsigned)braam_sys_sync(BRAAM_SYS_NOW, 0, 0, 0);
}

/* A Braam error as errno, and -1. */
static int failed(int status)
{
    errno = 32 - status;
    return -1;
}

/* A read of standard input flushes standard output first, as a terminal's line
   discipline would: a prompt is out before the answer is asked for. */
coro(braam_call *) ssize_t read(int fd, void *buf, size_t n)
{
    if (fd == 0)
        await fflush(stdout);
    if (n == 0)
        return 0;
    unsigned max = n > BRAAM_READ_MAX ? BRAAM_READ_MAX : (unsigned)n;
    unsigned got;
    int st = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_READ, fd), &max, 4, buf, max, &got);
    return st < 0 ? failed(st) : (ssize_t)got;
}

coro(braam_call *) ssize_t write(int fd, const void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        unsigned chunk = n - done > BRAAM_READ_MAX ? BRAAM_READ_MAX : (unsigned)(n - done);
        int st = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_WRITE, fd), (const char *)buf + done, chunk,
                                 NULL, 0, NULL);
        if (st < 0)
            return failed(st);
        if (st == 0)
            break;
        done += (unsigned)st;
    }
    return (ssize_t)done;
}

coro(braam_call *) int open(const char *path, int flags)
{
    int st =
        await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_OPEN, flags), path, strlen(path), NULL, 0, NULL);
    return st < 0 ? failed(st) : st;
}

coro(braam_call *) int close(int fd)
{
    int st = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_CLOSE, fd), NULL, 0, NULL, 0, NULL);
    return st < 0 ? failed(st) : 0;
}

coro(braam_call *) int sleep_ms(unsigned ms)
{
    int st = await braam_sys(BRAAM_SYS_SLEEP, &ms, 4, NULL, 0, NULL);
    return st < 0 ? failed(st) : 0;
}

static unsigned get_u32(const unsigned char *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

static unsigned long long get_u64(const unsigned char *p)
{
    return get_u32(p) | (unsigned long long)get_u32(p + 4) << 32;
}

coro(braam_call *) off_t lseek(int fd, off_t offset, int whence)
{
    unsigned char req[12], pos[8];
    unsigned got;
    unsigned long long o = (unsigned long long)offset;
    for (int i = 0; i < 4; i++)
        req[i] = (unsigned char)((unsigned)whence >> 8 * i);
    for (int i = 0; i < 8; i++)
        req[4 + i] = (unsigned char)(o >> 8 * i);
    int st = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_SEEK, fd), req, 12, pos, 8, &got);
    if (st < 0)
        return failed(st);
    return got == 8 ? (off_t)get_u64(pos) : failed(-8);
}

/* A call that names one path and answers with a status alone. */
static coro(braam_call *) int on_path(unsigned op, const char *path)
{
    int st = await braam_sys(op, path, strlen(path), NULL, 0, NULL);
    return st < 0 ? failed(st) : 0;
}

coro(braam_call *) int unlink(const char *path)
{
    return await on_path(BRAAM_SYS_REMOVE, path);
}

coro(braam_call *) int rmdir(const char *path)
{
    return await on_path(BRAAM_SYS_REMOVE, path);
}

coro(braam_call *) int remove(const char *path)
{
    return await on_path(BRAAM_SYS_REMOVE, path);
}

coro(braam_call *) int mkdir(const char *path, mode_t mode)
{
    (void)mode;
    return await on_path(BRAAM_SYS_MKDIR, path);
}

coro(braam_call *) int chdir(const char *path)
{
    return await on_path(BRAAM_SYS_OP(BRAAM_SYS_CHDIR, 1), path);
}

coro(braam_call *) char *getcwd(char *buf, size_t size)
{
    char cwd[FILENAME_MAX + 1];
    unsigned got;
    int st = await braam_sys(BRAAM_SYS_CHDIR, NULL, 0, cwd, FILENAME_MAX + 1, &got);
    if (st < 0) {
        failed(st);
        return NULL;
    }
    if (got >= size) {
        errno = ERANGE;
        return NULL;
    }
    memcpy(buf, cwd, got);
    buf[got] = 0;
    return buf;
}

/* rename's payload: u32 the old path's length, the old path, the new one. */
coro(braam_call *) int rename(const char *from, const char *to)
{
    unsigned a = strlen(from), b = strlen(to);
    unsigned char *req = malloc(4 + a + b);
    if (!req) {
        errno = ENOMEM;
        return -1;
    }
    defer free(req);
    for (int i = 0; i < 4; i++)
        req[i] = (unsigned char)(a >> 8 * i);
    memcpy(req + 4, from, a);
    memcpy(req + 4 + a, to, b);
    int st = await braam_sys(BRAAM_SYS_RENAME, req, 4 + a + b, NULL, 0, NULL);
    return st < 0 ? failed(st) : 0;
}

pid_t getpid(void)
{
    return braam_sys_sync(BRAAM_SYS_GETPID, 0, 0, 0);
}

/* Stat's reply, u32 kind, u64 size, u64 mtime, as a struct stat. */
static int fill(struct stat *st, const unsigned char *r, unsigned got, const char *path)
{
    if (got < 20)
        return failed(-8);
    unsigned kind = get_u32(r);
    memset(st, 0, sizeof *st);
    st->st_mode  = kind == BRAAM_KIND_DIR    ? S_IFDIR | 0755
                   : kind == BRAAM_KIND_LINK ? S_IFLNK | 0777
                                             : S_IFREG | 0644;
    st->st_size  = (off_t)get_u64(r + 4);
    st->st_mtime = (time_t)(get_u64(r + 12) / 1000);
    st->st_atime = st->st_ctime = st->st_mtime;
    st->st_dev                  = 1;
    st->st_nlink                = 1;
    st->st_blksize              = 512;
    st->st_blocks               = (st->st_size + 511) / 512;
    if (path) { /* FNV-1a, as Braam's compat layer makes st_ino */
        unsigned long long h = 0xcbf29ce484222325ull;
        for (; *path; path++)
            h = (h ^ (unsigned char)*path) * 0x100000001b3ull;
        st->st_ino = h;
    }
    return 0;
}

static coro(braam_call *) int stat_of(const char *path, struct stat *st, unsigned arg)
{
    unsigned char r[20];
    unsigned got;
    int s = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_STAT, arg), path, strlen(path), r, 20, &got);
    return s < 0 ? failed(s) : fill(st, r, got, path);
}

coro(braam_call *) int stat(const char *path, struct stat *st)
{
    return await stat_of(path, st, 0);
}

coro(braam_call *) int lstat(const char *path, struct stat *st)
{
    return await stat_of(path, st, BRAAM_STAT_NOFOLLOW);
}

coro(braam_call *) int fstat(int fd, struct stat *st)
{
    unsigned char r[20];
    unsigned got;
    int s = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_FSTAT, fd), NULL, 0, r, 20, &got);
    return s < 0 ? failed(s) : fill(st, r, got, NULL);
}

coro(braam_call *) int braam_yield(void)
{
    return await sleep_ms(0);
}

coro(braam_call *) int sig_catch(int sig, int on)
{
    static unsigned caught; /* the kernel's mask, shadowed so one bit can move alone */
    unsigned want = on ? caught | 1u << (sig & 31) : caught & ~(1u << (sig & 31));
    int st        = await braam_sys(BRAAM_SYS_SIGACT, &want, 4, NULL, 0, NULL);
    if (st < 0)
        return failed(st);
    caught = want;
    return 0;
}

coro(braam_call *) int poll(struct pollfd *fds, nfds_t n, int timeout)
{
    if (n > BRAAM_POLL_MAX) {
        errno = EINVAL;
        return -1;
    }
    unsigned req[1 + 2 * BRAAM_POLL_MAX], got[BRAAM_POLL_MAX], len;
    req[0] = timeout < 0 ? BRAAM_POLL_FOREVER : (unsigned)timeout;
    for (nfds_t i = 0; i < n; i++) {
        req[1 + 2 * i] = (unsigned)fds[i].fd;
        req[2 + 2 * i] = (unsigned)fds[i].events;
    }
    int st = await braam_sys(BRAAM_SYS_POLL, req, 4 + 8 * n, got, 4 * n, &len);
    if (st < 0)
        return failed(st);
    for (nfds_t i = 0; i < n; i++)
        fds[i].revents = 4 * i + 4 <= len ? (short)got[i] : 0;
    return st;
}

_Noreturn void _exit(int status)
{
    exit(status);
}

void __plain_write(int fd, const char *s); /* rt.c */

_Noreturn void __assert_fail(const char *expr, const char *file, int line)
{
    char msg[512];
    snprintf(msg, sizeof msg, "%s:%d: assertion failed: %s\n", file, line, expr);
    __plain_write(2, msg);
    abort();
}

/* Whether fd is the console's grid. */
coro(braam_call *) int isatty(int fd)
{
    unsigned char r[12];
    unsigned got;
    int st = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_TTY, fd), NULL, 0, r, 12, &got);
    if (st < 0) {
        failed(st);
        return 0;
    }
    return got >= 4 && (get_u32(r) & BRAAM_TTY_CONSOLE) ? 1 : 0;
}

/* Whether path exists: Braam keeps no permissions to check against mode. */
coro(braam_call *) int access(const char *path, int mode)
{
    (void)mode;
    struct stat st;
    return await stat(path, &st);
}

/* The zone of the last time(), in seconds east of UTC, for localtime. */
static long zone;

coro(braam_call *) time_t time(time_t *t)
{
    unsigned char r[12];
    unsigned got;
    int st = await braam_sys(BRAAM_SYS_CLOCK, NULL, 0, r, 12, &got);
    if (st < 0 || got < 12)
        return failed(st < 0 ? st : -8);
    time_t now = (time_t)(get_u64(r) / 1000);
    zone       = (long)(int)get_u32(r + 8) * 60;
    if (t)
        *t = now;
    return now;
}

struct tm *__civil(long long t, long offset, struct tm *tm);

struct tm *localtime(const time_t *t)
{
    static struct tm tm;
    return __civil(*t, zone, &tm);
}

/* A word list as the kernel takes it: u32 count, then u32 length and the bytes of
   each word.  Its size when to is NULL. */
static unsigned put_words(unsigned char *to, char *const *words)
{
    unsigned at = 4, n = 0;
    for (; words && words[n]; n++) {
        unsigned len = strlen(words[n]);
        if (to) {
            for (int i = 0; i < 4; i++)
                to[at + i] = (unsigned char)(len >> 8 * i);
            memcpy(to + at + 4, words[n], len);
        }
        at += 4 + len;
    }
    if (to)
        for (int i = 0; i < 4; i++)
            to[i] = (unsigned char)(n >> 8 * i);
    return at;
}

extern char **environ;

/* The child runs path, found along PATH when it has no slash, with argv but argv[0]
   replaced by path; its standard descriptors are this process's.  envp == environ
   lets it inherit the environment. */
static coro(braam_call *) int spawn_child(pid_t *pid, const char *path, char *const argv[],
                                          char *const envp[])
{
    int n = 0;
    while (argv && argv[n])
        n++;
    char **words = malloc((n + 2) * sizeof(char *));
    if (!words)
        return ENOMEM;
    defer free(words);
    words[0] = (char *)path;
    for (int k = 1; k < n; k++)
        words[k] = argv[k];
    words[n > 0 ? n : 1] = NULL;

    int inherit    = !envp || envp == environ;
    unsigned nargs = put_words(NULL, words), nenv = inherit ? 0 : put_words(NULL, envp);
    unsigned char *req = malloc(12 + nargs + nenv);
    if (!req)
        return ENOMEM;
    defer free(req);
    memset(req, 0, 12);
    req[4] = 1; /* fd1 */
    req[8] = 2; /* fd2 */
    put_words(req + 12, words);
    if (!inherit)
        put_words(req + 12 + nargs, envp);
    int st = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_SPAWN, inherit ? 0 : BRAAM_SPAWN_ENV), req,
                             12 + nargs + nenv, NULL, 0, NULL);
    if (st < 0)
        return 32 - st;
    if (pid)
        *pid = st;
    return 0;
}

coro(braam_call *) int posix_spawn(pid_t *pid, const char *path,
                                   const posix_spawn_file_actions_t *actions,
                                   const posix_spawnattr_t *attr, char *const argv[],
                                   char *const envp[])
{
    if (actions || attr)
        return ENOTSUP;
    return await spawn_child(pid, path, argv, envp);
}

coro(braam_call *) int posix_spawnp(pid_t *pid, const char *file,
                                    const posix_spawn_file_actions_t *actions,
                                    const posix_spawnattr_t *attr, char *const argv[],
                                    char *const envp[])
{
    return await posix_spawn(pid, file, actions, attr, argv, envp);
}

/* A child's end: Braam's status is an exit code, 130 for an interrupt and 132 for a
   trap included, so *status is always WIFEXITED. */
coro(braam_call *) pid_t waitpid(pid_t pid, int *status, int options)
{
    if (options) {
        errno = EINVAL;
        return -1;
    }
    unsigned char r[4];
    unsigned got;
    unsigned arg = pid == -1 ? BRAAM_WAIT_ANY : (unsigned)pid;
    int st       = await braam_sys(BRAAM_SYS_OP(BRAAM_SYS_WAIT, arg), NULL, 0, r, 4, &got);
    if (st < 0)
        return failed(st);
    if (got < 4)
        return failed(-8);
    if (status)
        *status = (st & 0xff) << 8;
    return (pid_t)get_u32(r);
}

coro(braam_call *) pid_t wait(int *status)
{
    return await waitpid(-1, status, 0);
}

/* The template's six X before a suffix of `suffixlen` bytes, replaced until a new
   file can be made; its descriptor, open for reading and writing. */
coro(braam_call *) int mkstemps(char *template, int suffixlen)
{
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t len                  = strlen(template);
    if (suffixlen < 0 || len < 6 + (size_t)suffixlen ||
        memcmp(template + len - suffixlen - 6, "XXXXXX", 6) != 0) {
        errno = EINVAL;
        return -1;
    }
    char *x = template + len - suffixlen - 6;
    for (int tries = 0; tries < 100; tries++) {
        unsigned bits = (unsigned)braam_sys_sync(BRAAM_SYS_RANDOM, 0, 0, 0);
        unsigned more = (unsigned)braam_sys_sync(BRAAM_SYS_RANDOM, 0, 0, 0);
        for (int i = 0; i < 6; i++) {
            x[i] = letters[(i < 4 ? bits : more) % 62];
            if (i < 4)
                bits /= 62;
            else
                more /= 62;
        }
        int fd = await open(template, O_RDWR | O_CREAT | O_EXCL);
        if (fd >= 0 || errno != EEXIST)
            return fd;
    }
    errno = EEXIST;
    return -1;
}

coro(braam_call *) int mkstemp(char *template)
{
    return await mkstemps(template, 0);
}

coro(braam_call *) ssize_t readlink(const char *path, char *buf, size_t size)
{
    char target[PATH_MAX];
    unsigned got;
    int st = await braam_sys(BRAAM_SYS_READLINK, path, strlen(path), target, sizeof target, &got);
    if (st < 0)
        return failed(st);
    if (got > size)
        got = size;
    memcpy(buf, target, got);
    return (ssize_t)got;
}

/* Appends the components of `rest` to the absolute path `out`, of `len` bytes,
   following links as they are met: the new length, or -1. */
static coro(braam_call *) int walk(char *out, int len, const char *rest, int *links)
{
    while (*rest) {
        while (*rest == '/')
            rest++;
        size_t n = strcspn(rest, "/");
        if (n == 0 || (n == 1 && rest[0] == '.')) {
            rest += n;
            continue;
        }
        if (n == 2 && rest[0] == '.' && rest[1] == '.') {
            while (len > 0 && out[len - 1] != '/')
                len--;
            if (len > 1)
                len--; /* the slash, but for the root's */
            out[len] = 0;
            rest += n;
            continue;
        }
        if (len + 1 + (int)n >= PATH_MAX) {
            errno = EINVAL; /* longer than PATH_MAX */
            return -1;
        }
        int was = len;
        if (len > 1)
            out[len++] = '/';
        memcpy(out + len, rest, n);
        len += (int)n;
        out[len] = 0;
        rest += n;

        struct stat st;
        if (await lstat(out, &st) < 0)
            return -1;
        if (S_ISLNK(st.st_mode)) {
            char target[PATH_MAX];
            ssize_t k = await readlink(out, target, sizeof target - 1);
            if (k < 0)
                return -1;
            if (++*links > 40) {
                errno = ELOOP;
                return -1;
            }
            target[k] = 0;
            if (target[0] == '/') {
                len = 1;
                strcpy(out, "/");
            } else {
                len      = was;
                out[len] = 0;
                if (len == 0) {
                    strcpy(out, "/");
                    len = 1;
                }
            }
            len = await walk(out, len, target, links);
            if (len < 0)
                return -1;
        }
    }
    return len;
}

coro(braam_call *) char *realpath(const char *path, char *resolved)
{
    char out[PATH_MAX];
    int len = 1, links = 0;
    strcpy(out, "/");
    if (path[0] != '/') {
        if (!await getcwd(out, sizeof out))
            return NULL;
        len = (int)strlen(out);
    }
    len = await walk(out, len, path, &links);
    if (len < 0)
        return NULL;
    if (!resolved && !(resolved = malloc(len + 1))) {
        errno = ENOMEM;
        return NULL;
    }
    return memcpy(resolved, out, len + 1);
}
