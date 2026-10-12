//
// The files cpp reads. On the host these are the system's calls. On Braam a
// call that waits is a coroutine, and cpp reads from deep inside its scanner, so
// before preprocessing begins the input and every file it might include are
// read whole, and these calls serve them from memory. Which file an #include
// names depends on macros, so the prefetch takes a superset: every name in an
// #include line of every file loaded, under every directory of the search path,
// whatever the conditionals around it. A computed #include (a macro naming the
// file) cannot be foreseen; on Braam its file is not found.
//
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "intern.h"

#ifndef __braam__

coro(braam_call *) int cpp_load(const char *path)
{
    (void)path;
    return 0;
}

coro(braam_call *) void cpp_preload(void)
{
}

int cpp_open(const char *path)
{
    return open(path, READ);
}

int cpp_read(int fd, char *buf, int n)
{
    return read(fd, buf, n);
}

void cpp_close(int fd)
{
    close(fd);
}

int cpp_fstat(int fd, struct stat *st)
{
    return fstat(fd, st);
}

#else

coro(braam_call *) char *read_file(const char *path, size_t *len); // libutil

#define MAXFILES 512
#define FD_BASE  100 // above anything the process has open
#define MAXOPEN  (MAXINC + 4)

static struct {
    char *path;
    char *text; // NULL: tried, and not there
    size_t len;
} files[MAXFILES];
static int nfiles;

static struct {
    int file; // 1 + the index in files[], or 0 for a free slot
    size_t pos;
} opened[MAXOPEN];

static int find(const char *path)
{
    for (int i = 0; i < nfiles; i++)
        if (strcmp(files[i].path, path) == 0)
            return i;
    return -1;
}

// Load path, once: its index, or -1 when it cannot be read.
static coro(braam_call *) int load(const char *path)
{
    int i = find(path);
    if (i >= 0)
        return files[i].text ? i : -1;
    if (nfiles == MAXFILES) {
        errno = ENOMEM;
        return -1;
    }
    i             = nfiles++;
    files[i].path = strdup(path);
    files[i].text = await read_file(path, &files[i].len);
    return files[i].text ? i : -1;
}

coro(braam_call *) int cpp_load(const char *path)
{
    return await load(path) < 0 ? -1 : 0;
}

static coro(braam_call *) void scan(const char *text, size_t len, const char *dir);

// A name an #include may give, under each directory it may be looked for in, as
// do_include() spells the path: the includer's own directory, then the search
// path. Each file found is scanned in its turn.
static coro(braam_call *) void try_name(const char *name, const char *dir)
{
    char path[1024];
    for (int k = 0; k < cpp.ndirs; k++) {
        const char *d = k == 0 ? dir : cpp.search_dirs[k];
        if (!d)
            break;
        if (name[0] == '/' || *d == '\0')
            snprintf(path, sizeof path, "%s", name);
        else
            snprintf(path, sizeof path, "%s/%s", d, name);
        if (find(path) >= 0)
            continue; // tried, and scanned if it was there
        int i = await load(path);
        if (i >= 0) {
            char sub[1024];
            snprintf(sub, sizeof sub, "%s", path);
            await scan(files[i].text, files[i].len, dir_of(sub));
        }
    }
}

// Every name in an '#include "name"' or '#include <name>' line of text, whose
// file is in dir.
static coro(braam_call *) void scan(const char *text, size_t len, const char *dir)
{
    char own[1024];
    snprintf(own, sizeof own, "%s", dir);
    for (size_t at = 0; at < len; at++) {
        if (at > 0 && text[at - 1] != '\n')
            continue;
        size_t p = at;
        while (p < len && (text[p] == ' ' || text[p] == '\t'))
            p++;
        if (p == len || text[p] != '#')
            continue;
        p++;
        while (p < len && (text[p] == ' ' || text[p] == '\t'))
            p++;
        if (len - p < 7 || strncmp(text + p, "include", 7) != 0)
            continue;
        p += 7;
        while (p < len && (text[p] == ' ' || text[p] == '\t'))
            p++;
        if (p == len || (text[p] != '"' && text[p] != '<'))
            continue;
        char end = text[p] == '"' ? '"' : '>';
        size_t q = ++p;
        while (q < len && text[q] != end && text[q] != '\n')
            q++;
        if (q == len || text[q] != end || q - p >= 512)
            continue;
        char name[512];
        memcpy(name, text + p, q - p);
        name[q - p] = '\0';
        await try_name(name, own);
    }
}

coro(braam_call *) void cpp_preload(void)
{
    if (cpp.in_fd == STDIN) { // no input file: standard input, read whole
        size_t cap = 4096, len = 0;
        char *text = malloc(cap);
        ssize_t n;
        while (text && (n = await read(STDIN, text + len, cap - len)) > 0)
            if ((len += n) == cap)
                text = realloc(text, cap *= 2);
        if (!text || nfiles == MAXFILES)
            return;
        files[nfiles].path = "<stdin>";
        files[nfiles].text = text;
        files[nfiles].len  = len;
        nfiles++;
        cpp.in_fd = cpp.inc_fd[0] = cpp_open("<stdin>");
        return;
    }
    int i = find(cpp.inc_file[0]);
    if (i >= 0 && files[i].text)
        await scan(files[i].text, files[i].len, cpp.inc_dir[0]);
}

int cpp_open(const char *path)
{
    int i = find(path);
    if (i < 0 || !files[i].text) {
        errno = ENOENT;
        return -1;
    }
    for (int k = 0; k < MAXOPEN; k++)
        if (opened[k].file == 0) {
            opened[k].file = i + 1;
            opened[k].pos  = 0;
            return FD_BASE + k;
        }
    errno = EINVAL;
    return -1;
}

// The file open on fd, or -1.
static int file_of(int fd)
{
    int k = fd - FD_BASE;
    if (k < 0 || k >= MAXOPEN || opened[k].file == 0) {
        errno = EINVAL;
        return -1;
    }
    return opened[k].file - 1;
}

int cpp_read(int fd, char *buf, int n)
{
    int i = file_of(fd);
    if (i < 0)
        return -1;
    size_t *pos = &opened[fd - FD_BASE].pos;
    if ((size_t)n > files[i].len - *pos)
        n = (int)(files[i].len - *pos);
    memcpy(buf, files[i].text + *pos, n);
    *pos += n;
    return n;
}

void cpp_close(int fd)
{
    if (file_of(fd) >= 0)
        opened[fd - FD_BASE].file = 0;
}

// The path with "." and ".." taken out, so two spellings of one file agree.
static void normalize(const char *path, char *out, size_t size)
{
    char copy[1024];
    const char *parts[128];
    int n = 0;
    snprintf(copy, sizeof copy, "%s", path);
    for (char *p = strtok(copy, "/"); p; p = strtok(NULL, "/")) {
        if (strcmp(p, ".") == 0)
            continue;
        if (strcmp(p, "..") == 0 && n > 0 && strcmp(parts[n - 1], "..") != 0)
            n--;
        else if (n < 128)
            parts[n++] = p;
    }
    size_t at = 0;
    if (path[0] == '/')
        at += snprintf(out + at, size - at, "/");
    for (int k = 0; k < n && at < size; k++)
        at += snprintf(out + at, size - at, "%s%s", k ? "/" : "", parts[k]);
    if (at == 0)
        snprintf(out, size, ".");
}

// For #pragma once: a file's identity is its path, normalized and hashed as
// Braam's stat makes st_ino.
int cpp_fstat(int fd, struct stat *st)
{
    int i = file_of(fd);
    if (i < 0)
        return -1;
    char path[1024];
    normalize(files[i].path, path, sizeof path);
    memset(st, 0, sizeof *st);
    st->st_dev           = 1;
    unsigned long long h = 0xcbf29ce484222325ull; // FNV-1a
    for (const char *p = path; *p; p++)
        h = (h ^ (unsigned char)*p) * 0x100000001b3ull;
    st->st_ino  = h;
    st->st_size = (off_t)files[i].len;
    return 0;
}

#endif
