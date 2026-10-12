/*
 * The Braam process runtime (docs/Braam.md §7.2), as braam-core's src/proc/rt.cpp
 * is in C++: a handful of tasks, each a coroutine yielding braam_call *, and one
 * outstanding call each.  Task 0 is the root, which awaits main and then flushes what
 * main left buffered; braam_spawn adds the others.  When a task suspends, the
 * braam_call it yielded goes to the kernel with a token of its own, and _resume brings
 * the reply back to that task.  The process ends when the root returns.  The exports
 * themselves are in exports.s.
 */
#include <braam.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

coro(braam_call *) int main(int argc, char **argv);
void __braam_sys_async(unsigned op, unsigned token, const void *ptr, unsigned len);

/* The size of the block for the root's frame and the arena of everything main awaits:
   64 KiB (taskbytes.c), or the program's own definition. */
extern const unsigned __braam_task_bytes;

static coro(braam_call *) int braam_root(int argc, char **argv)
{
    int status = await main(argc, argv);
    await fflush(NULL);
    return status;
}

/* The tasks: a frame, and the call it waits on (NULL while it runs). */
static struct {
    braam_task *frame;
    braam_call *call;
} tasks[BRAAM_TASKS];
static unsigned token;
static int exited;
static unsigned signals; /* what _sig recorded and sig_take has not collected */

/* Runs task k to its next suspension, and hands the kernel what it yielded; a task
   that returns leaves the table, and the root's return is the exit. */
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

/* What _start and _resume return: 0 = exited, 1 = suspended. */
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

/* argv from the blob the host placed: u32 argc, then u32 length and the bytes of each
   word.  The words are copied, each with its NUL; the environment after them is not
   read yet. */
static char **parse_argv(const unsigned char *blob, unsigned len, int *argc)
{
    unsigned n = len >= 4 ? get_u32(blob) : 0, at = 4, bytes = 0;
    unsigned k;
    for (k = 0; k < n && at + 4 <= len; k++) {
        unsigned size = get_u32(blob + at);
        if (size > len - at - 4)
            break;
        at += 4 + size;
        bytes += size + 1;
    }
    n           = k;
    char **argv = malloc((n + 1) * sizeof(char *) + bytes);
    if (!argv) {
        *argc = 0;
        return NULL;
    }
    char *to = (char *)(argv + n + 1);
    at       = 4;
    for (k = 0; k < n; k++) {
        unsigned size = get_u32(blob + at);
        memcpy(to, blob + at + 4, size);
        to[size] = 0;
        argv[k]  = to;
        to += size + 1;
        at += 4 + size;
    }
    argv[n] = NULL;
    *argc   = (int)n;
    return argv;
}

int __braam_start(unsigned blob, unsigned len)
{
    if (tasks[0].frame)
        return state();
    int argc;
    char **argv   = parse_argv((const unsigned char *)blob, len, &argc);
    void *storage = malloc(__braam_task_bytes); /* 16-aligned */
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
    /* An answer nobody waits for: its call went with a destroyed frame. */
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

/* A call whose coroutine is gone before its answer came: the task it belonged to was
   destroyed, so its slot is free again.  A call answered has already left the table. */
void __braam_forget(braam_call *c)
{
    for (int k = 0; k < BRAAM_TASKS; k++)
        if (tasks[k].call == c) {
            tasks[k].call  = NULL;
            tasks[k].frame = NULL;
        }
}
