// The Braam process runtime (docs/Braam.md §7.2), as braam-core's src/proc/rt.cpp
// is in C++: a handful of tasks, each a coroutine yielding braam_call *, and one
// outstanding call each.  Task 0 is the root, which awaits main and then flushes what
// main left buffered; braam_spawn adds the others.  When a task suspends, the
// braam_call it yielded goes to the kernel with a token of its own, and _resume brings
// the reply back to that task.  The process ends when the root returns.  The exports
// themselves are in exports.s.
#include <braam.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

coro(braam_call *) int main(int argc, char **argv);
void __braam_sys_async(unsigned op, unsigned token, const void *ptr, unsigned len);

// The size of the block for the root's frame and the arena of everything main awaits:
// 64 KiB (taskbytes.c), or the program's own definition.
extern const unsigned __braam_task_bytes;

// What atexit registered, run in reverse when main returns or exit() is called.
static void (*at_exit[32])(void);
static int n_at_exit;

static coro(braam_call *) int braam_root(int argc, char **argv)
{
    int status = await main(argc, argv);
    while (n_at_exit > 0)
        at_exit[--n_at_exit]();
    await fflush(NULL);
    return status;
}

// The tasks: a frame, and the call it waits on (NULL while it runs).
static struct {
    braam_task *frame;
    braam_call *call;
} tasks[BRAAM_TASKS];
static unsigned token;
static int exited;
static unsigned signals; // what _sig recorded and sig_take has not collected

// Runs task k to its next suspension, and hands the kernel what it yielded; a task
// that returns leaves the table, and the root's return is the exit.
static void run(int k)
{
    if (co_resume(tasks[k].frame) == CO_DONE) {
        if (k == 0) {
            exited = 1;
            braam_sys_sync(BRAAM_SYS_EXIT, (unsigned)co_result(tasks[0].frame), 0, 0);
        } else {
            tasks[k].frame = NULL;
        }
        return;
    }
    braam_call *c = co_value(tasks[k].frame);
    c->token      = ++token;
    tasks[k].call = c;
    __braam_sys_async(c->op, c->token, c->ptr, c->len);
}

// What _start and _resume return: 0 = exited, 1 = suspended.
static int state(void)
{
    return exited ? 0 : 1;
}

int braam_spawn(braam_task *frame)
{
    for (int k = 1; k < BRAAM_TASKS; k++)
        if (!tasks[k].frame) {
            tasks[k].frame = frame;
            tasks[k].call  = NULL;
            run(k);
            return k;
        }
    return 0;
}

static unsigned get_u32(const unsigned char *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

// A word list from the blob the host placed, at *at: u32 count, then u32 length and the
// bytes of each word.  The words are copied, each with its NUL, into one block from
// malloc, the vector first; *at moves past the list.
static char **parse_words(const unsigned char *blob, unsigned len, unsigned *at, int *count)
{
    unsigned n = *at + 4 <= len ? get_u32(blob + *at) : 0, p = *at + 4, bytes = 0;
    unsigned k;
    for (k = 0; k < n && p + 4 <= len; k++) {
        unsigned size = get_u32(blob + p);
        if (size > len - p - 4)
            break;
        p += 4 + size;
        bytes += size + 1;
    }
    n           = k;
    char **list = malloc((n + 1) * sizeof(char *) + bytes);
    if (!list) {
        *count = 0;
        return NULL;
    }
    char *to = (char *)(list + n + 1);
    p        = *at + 4;
    for (k = 0; k < n; k++) {
        unsigned size = get_u32(blob + p);
        memcpy(to, blob + p + 4, size);
        to[size] = 0;
        list[k]  = to;
        to += size + 1;
        p += 4 + size;
    }
    list[n] = NULL;
    *count  = (int)n;
    *at     = p;
    return list;
}

// The environment, NAME=value words, after argv in the blob.
char **environ;

char *getenv(const char *name)
{
    size_t n = strlen(name);
    for (char **e = environ; e && *e; e++)
        if (strncmp(*e, name, n) == 0 && (*e)[n] == '=')
            return *e + n + 1;
    return NULL;
}

int atexit(void (*func)(void))
{
    if (n_at_exit == 32)
        return -1;
    at_exit[n_at_exit++] = func;
    return 0;
}

int __braam_start(unsigned blob, unsigned len)
{
    if (tasks[0].frame)
        return state();
    int argc, envc;
    unsigned at   = 0;
    char **argv   = parse_words((const unsigned char *)blob, len, &at, &argc);
    environ       = parse_words((const unsigned char *)blob, len, &at, &envc);
    void *storage = malloc(__braam_task_bytes); // 16-aligned
    if (!storage) {
        exited = 1;
        braam_sys_sync(BRAAM_SYS_EXIT, 1, 0, 0);
        return 0;
    }
    tasks[0].frame = co_init(storage, __braam_task_bytes, braam_root, argc, argv);
    run(0);
    return state();
}

int __braam_resume(unsigned tok, unsigned reply, unsigned len)
{
    for (int k = 0; k < BRAAM_TASKS && !exited; k++) {
        braam_call *c = tasks[k].call;
        if (tasks[k].frame && c && c->token == tok) {
            c->reply      = (void *)reply;
            c->reply_len  = len;
            tasks[k].call = NULL;
            run(k);
            return state();
        }
    }
    // An answer nobody waits for: its call went with a destroyed frame.
    free((void *)reply);
    return state();
}

void __braam_signal(unsigned n)
{
    signals |= 1u << (n & 31);
}

unsigned sig_pending(void)
{
    return signals;
}

int sig_take(int sig)
{
    unsigned bit = 1u << (sig & 31);
    if (!(signals & bit))
        return 0;
    signals &= ~bit;
    return 1;
}

// A call whose coroutine is gone before its answer came: the task it belonged to was
// destroyed, so its slot is free again.  A call answered has already left the table.
void __braam_forget(braam_call *c)
{
    for (int k = 0; k < BRAAM_TASKS; k++)
        if (tasks[k].call == c) {
            tasks[k].call  = NULL;
            tasks[k].frame = NULL;
        }
}

// Posts a call nobody waits for: its payload is copied at once, and the kernel
// performs a Write posted in a process's last step before it goes.
void __braam_post(unsigned op, const void *ptr, unsigned len)
{
    __braam_sys_async(op, ++token, ptr, len);
}

void __braam_exit(int status); // exports.s
void __stdio_post(void);       // stdio.c

// A write from plain code, which cannot await one: an assertion, a coroutine trap,
// getopt's complaint, a diagnostic held to the end.  Posted to `fd` after what the
// streams hold.
void __plain_write(int fd, const char *s)
{
    __stdio_post();
    __braam_post(BRAAM_SYS_OP(BRAAM_SYS_WRITE, fd), s, (unsigned)strlen(s));
}

// An exit from anywhere, plain code included: nothing unwinds, so the atexit
// functions run and the stdio buffers are posted, and the process ends in this
// step.
_Noreturn void exit(int status)
{
    while (n_at_exit > 0)
        at_exit[--n_at_exit]();
    __stdio_post();
    __braam_exit(status);
}

_Noreturn void _Exit(int status)
{
    __braam_exit(status);
}

// Status 134 (128 + SIGABRT), as a shell reports it; what was written still goes.
_Noreturn void abort(void)
{
    __stdio_post();
    __braam_exit(134);
}
