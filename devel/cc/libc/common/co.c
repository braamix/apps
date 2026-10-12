/*
 * The coroutine runtime (docs/Coroutines_in_C.md; docs/Coroutines_Internals.md §5).  The
 * compiler lowers a coroutine f to f$resume, the body as a state machine over a frame,
 * f$init, which stores its arguments there, and f$co, its frame size and alignment
 * (then its init and resume too, for a coroutine with a coro_ptr).
 * An await and a co_alloca in a coroutine take their frames off the arena of the task,
 * the storage of its root frame after the frame itself, in LIFO order.
 * The frame starts with this header; the rest is the defining unit's business.
 *
 * The header's layout follows the target's unsigned and pointers (translator/coro.c,
 * co_header_size).  A trap prints its name and ends the program with status 255.
 */
#include <limits.h>
#include <stddef.h>

struct co_header {
    unsigned state;         /* 0 created; k >= 1 suspended at point k; DONE; DESTROYED */
    unsigned flags;         /* RUNNING, and the signal of the resumption, shifted by 1 */
    int (*resume)(void *);  /* f$resume */
    struct co_header *task; /* the root frame of the task */
    char *top, *limit;      /* the root's arena */
};

enum {
    RUNNING        = 1,
    SIGNAL_DESTROY = 2,
};
#define STATE_DONE      (UINT_MAX - 1)
#define STATE_DESTROYED UINT_MAX

void __plain_write(int fd, const char *s); /* the target's runtime: a write from plain code */
_Noreturn void exit(int status);

static void print(const char *s)
{
    __plain_write(1, s);
}

/* The trap `name`, and the coroutine it concerns when there is one. */
_Noreturn void __coro_trap(const char *name, const char *coroutine)
{
    print("coroutine trap: ");
    print(name);
    if (coroutine) {
        print(": ");
        print(coroutine);
    }
    print("\n");
    exit(255);
}

_Noreturn static void trap(const char *name)
{
    __coro_trap(name, 0);
}

/* co_init, co_alloca and an arena await: a frame of desc[0] bytes aligned to desc[1] at
   the front of `bytes` of storage, the rest its arena.  With no parent it is the root of
   a task of its own; an arena await's belongs to the task of its parent, the awaiter. */
void *__coro_setup(void *storage, size_t bytes, const size_t *desc, int (*resume)(void *),
                   void *parent)
{
    struct co_header *h = storage;
    if (((size_t)storage & (desc[1] - 1)) != 0 || bytes < desc[0])
        trap("CO_TRAP_STORAGE");
    h->state  = 0;
    h->flags  = 0;
    h->resume = resume;
    h->task   = parent ? ((struct co_header *)parent)->task : h;
    h->top    = (char *)storage + desc[0];
    h->limit  = (char *)storage + bytes;
    return storage;
}

/* co_resume, co_cancel and co_destroy: signal 0, 1 and 2.  Returns the co_status. */
int __coro_resume(void *p, int signal)
{
    struct co_header *h = p;
    if (h->state >= STATE_DONE)
        trap("CO_TRAP_FINISHED");
    if (h->flags & RUNNING)
        trap("CO_TRAP_REENTRANT");
    if (signal == SIGNAL_DESTROY && h->state == 0) {
        h->state = STATE_DESTROYED; /* never started: no defer to run */
        return 1;
    }
    h->flags   = RUNNING | (unsigned)signal << 1;
    int status = h->resume(h);
    h->flags   = 0;
    return status;
}

int __coro_done(void *p)
{
    return ((struct co_header *)p)->state >= STATE_DONE;
}

/* The address of the last value yielded, `off` bytes into the frame. */
void *__coro_value(void *p, unsigned off)
{
    unsigned state = ((struct co_header *)p)->state;
    if (state == 0 || state >= STATE_DONE)
        trap("CO_TRAP_NO_VALUE");
    return (char *)p + off;
}

/* The address of the result, `off` bytes into the frame. */
void *__coro_result(void *p, unsigned off)
{
    if (((struct co_header *)p)->state != STATE_DONE)
        trap("CO_TRAP_NOT_DONE");
    return (char *)p + off;
}

/* An arena await and a co_alloca in a coroutine: `bytes` aligned to `align` off the
   arena of the task frame `fp` belongs to, for a frame of coroutine `name`; given back
   by __coro_pop in LIFO order. */
void *__coro_push(void *fp, size_t bytes, size_t align, const char *name)
{
    struct co_header *t = ((struct co_header *)fp)->task;
    char *p             = (char *)(((size_t)t->top + align - 1) & -align);
    if (p > t->limit || bytes > (size_t)(t->limit - p))
        __coro_trap("CO_TRAP_NO_SPACE", name);
    t->top = p + bytes;
    return p;
}

void __coro_pop(void *fp, void *p)
{
    ((struct co_header *)fp)->task->top = p;
}

/* A variadic coroutine's f$init: a copy of the `size` bytes of its variable arguments
   at `va`, off the arena of the task frame `fp` belongs to, right above the frame, so
   that the frame's release gives it back too; or NULL when there are none. */
char *__coro_va_save(void *fp, const char *va, size_t size)
{
    if (!size)
        return 0;
    char *p = __coro_push(fp, size, 16, "variable arguments");
    for (size_t i = 0; i < size; i++)
        p[i] = va[i];
    return p;
}

/* An arena await of the coroutine desc and resume describe: its frame off the arena of
   the awaiting frame fp's task, set up to join that task. */
void *__coro_start(void *fp, const size_t *desc, int (*resume)(void *), const char *name)
{
    void *sub = __coro_push(fp, desc[0], desc[1], name);
    return __coro_setup(sub, desc[0], desc, resume, fp);
}

/* A step of an await: the awaited frame sub resumed with signal 0 or 1.  Suspended, the
   value it yielded, size bytes at value_off, becomes the awaiting frame fp's, at the
   same offset since the two yield one type.  Returns the co_status. */
int __coro_step(void *sub, int signal, void *fp, unsigned value_off, unsigned size)
{
    int status = __coro_resume(sub, signal);
    if (status == 0)
        for (unsigned i = 0; i < size; i++)
            ((char *)fp)[value_off + i] = ((char *)sub)[value_off + i];
    return status;
}

/* An await whose awaiting frame fp is being destroyed: the awaited frame sub is
   destroyed first, and given back to the arena when it came from there. */
void __coro_drop(void *fp, void *sub, int arena)
{
    __coro_resume(sub, SIGNAL_DESTROY);
    if (arena)
        __coro_pop(fp, sub);
}
