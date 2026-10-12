/*
 * <braam.h> — the Braam process ABI (docs/Braam.md §7; braam-core's
 * src/kernel/sysabi.h and doc/System_Calls.md).
 *
 * A process makes a request by yielding a braam_call: the runtime (libc/wasm32/braam/
 * rt.c) hands it to the kernel and resumes the coroutine with the reply.  braam_sys is
 * the one primitive; read, write, open, close, sleep_ms and fflush are built on it.  A
 * function that calls one is itself a coroutine yielding braam_call *, up to main:
 *
 *     coro(braam_call *) int main(int argc, char **argv);
 *
 * The numbers below are transcribed from sysabi.h; the braam-abi test compares them
 * with braam-core's when that tree is beside this one.
 */
#ifndef _BRAAM_H
#define _BRAAM_H

#include <coro.h>
#include <stddef.h>

/* One request, from the call to the answer. */
typedef struct braam_call {
    unsigned op, token; /* the op word; the token the runtime gave it */
    const void *ptr;    /* the request's payload */
    unsigned len;
    void *reply; /* the block the host placed: an i32 status, then the data */
    unsigned reply_len;
} braam_call;

/* The process metadata `exec` reads: the braam custom section. */
#define BRAAM_PROC_MAGIC     0x6d617262 /* "bram" */
#define BRAAM_PROC_ABI       21
#define BRAAM_PROC_PAGE      65536
#define BRAAM_PROC_MAX_PAGES 1600

/* The synchronous calls: kernel.sys(op, a0, a1, a2). */
#define BRAAM_SYS_EXIT   1
#define BRAAM_SYS_GETPID 2
#define BRAAM_SYS_NOW    3
#define BRAAM_SYS_STAGE  4
#define BRAAM_SYS_RANDOM 5

/* The asynchronous ones, through braam_sys; the argument in the op word's upper bits. */
#define BRAAM_SYS_WRITE    16 /* arg fd; payload the bytes; status bytes written */
#define BRAAM_SYS_READ     17 /* arg fd; payload empty or u32 max; data the chunk */
#define BRAAM_SYS_OPEN     18 /* arg BRAAM_O_*; payload the path; status the fd */
#define BRAAM_SYS_CLOSE    19 /* arg fd */
#define BRAAM_SYS_STAT     20 /* payload the path; data u32 kind, u64 size, u64 mtime */
#define BRAAM_SYS_LIST     21
#define BRAAM_SYS_MKDIR    22
#define BRAAM_SYS_REMOVE   23
#define BRAAM_SYS_TOUCH    24
#define BRAAM_SYS_CHDIR    25
#define BRAAM_SYS_DUP      26
#define BRAAM_SYS_RENAME   29 /* payload u32 from_len, the old path, the new one */
#define BRAAM_SYS_SEEK     30 /* arg fd; payload u32 whence, i64 offset; data u64 position */
#define BRAAM_SYS_TRUNCATE 31
#define BRAAM_SYS_SLEEP    32 /* payload u32 milliseconds */
#define BRAAM_SYS_FSTAT    33 /* arg fd; data as Stat's */
#define BRAAM_SYS_SIGACT   85 /* payload u32 mask, or empty to ask; data u32 the mask before */
#define BRAAM_SYS_POLL     86 /* payload u32 timeout, then u32 fd, u32 events pairs */

#define BRAAM_SYS_OP(op, arg) ((unsigned)(op) | (unsigned)(arg) << 8)

/* Sys::Open's flags. */
#define BRAAM_O_READ   1
#define BRAAM_O_WRITE  2
#define BRAAM_O_CREATE 4
#define BRAAM_O_TRUNC  8
#define BRAAM_O_APPEND 16
#define BRAAM_O_EXCL   32

/* Stat's kinds, and its argument. */
#define BRAAM_KIND_FILE     0
#define BRAAM_KIND_DIR      1
#define BRAAM_KIND_LINK     2
#define BRAAM_STAT_NOFOLLOW 1

/* Signals, Unix's numbers; only INT, TERM and WINCH may be caught. */
#define BRAAM_SIG_INT   2
#define BRAAM_SIG_KILL  9
#define BRAAM_SIG_TERM  15
#define BRAAM_SIG_CONT  18
#define BRAAM_SIG_TSTP  20
#define BRAAM_SIG_WINCH 28

/* Poll's events, and its timeout that waits for ever; the most descriptors it takes. */
#define BRAAM_POLL_IN      1
#define BRAAM_POLL_OUT     2
#define BRAAM_POLL_HUP     4
#define BRAAM_POLL_FOREVER 0xffffffff
#define BRAAM_POLL_MAX     64

/* A read with no length; the most one may ask for. */
#define BRAAM_CHUNK    512
#define BRAAM_READ_MAX (65536 - 4)

/* A synchronous call. */
int braam_sys_sync(unsigned op, unsigned a0, unsigned a1, unsigned a2);

/*
 * An asynchronous call: `len` bytes of payload at `payload`.  Returns the reply's
 * status, negative a Braam error (-1 Invalid, -3 NotFound, ...), and copies up to `cap`
 * bytes of its data to `out`, their number to *got (when got is not NULL).
 */
coro(braam_call *) int braam_sys(unsigned op, const void *payload, unsigned len, void *out,
                                 unsigned cap, unsigned *got);

/* Wait `ms` milliseconds; 0 parks the process once, so others may run. */
coro(braam_call *) int sleep_ms(unsigned ms);

/* Milliseconds since boot. */
unsigned braam_now(void);

/* Park once, so a signal can reach a program that computes for long (as sleep_ms(0)). */
coro(braam_call *) int braam_yield(void);

/*
 * Tasks.  A process has up to BRAAM_TASKS of them, each a coroutine of this type with
 * one call outstanding; main runs in task 0, and the process ends when main returns,
 * whatever the others are doing.  braam_spawn takes a frame made by co_init (or
 * co_alloca, in a block that outlives the task), runs it to its first suspension, and
 * from then on resumes it whenever its call is answered.  It returns the task's
 * number, or 0 when the table is full.  A task that returns leaves the table; its
 * result is in its frame.  A task destroyed while it waits leaves the table too.
 */
#define BRAAM_TASKS 8
typedef co_frame(braam_call *, int) braam_task;
int braam_spawn(braam_task *frame);

/*
 * Signals.  A process is told only of the signals it asked for; any other one runs
 * its default action, which ends the process with status 130.  A signal asked for is
 * recorded, and the calls the process is parked on that can wait for ever (read of a
 * terminal or a pipe, sleep_ms, braam_yield, poll) give up with EINTR.  sig_take then
 * says whether `sig` came, and forgets it.  A signal reaches a process only while it
 * is parked: a loop that awaits nothing cannot be interrupted.
 */
coro(braam_call *) int sig_catch(int sig, int on); /* 0, or -1 with errno set */
int sig_take(int sig);
unsigned sig_pending(void); /* the bits 1 << sig not taken */

#endif /* _BRAAM_H */
