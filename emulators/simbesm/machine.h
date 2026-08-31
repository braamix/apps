/*
 * The machine: types, devices, the event queue and the driver.
 *
 * Copyright (c) 1993-2022, Robert M Supnik and the Open SIMH contributors
 * Copyright (c) 2026, Serge Vakulenko
 *
 * What is left of the SIMH framework.  Types and structures are upstream's,
 * because besm6/ is written against them.
 */
#ifndef BESM6_MACHINE_H
#define BESM6_MACHINE_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

typedef int32_t int32;
typedef uint8_t uint8;
typedef uint32_t uint32;

typedef int t_stat; /* status */
typedef int t_bool; /* boolean */

typedef signed long long t_int64;
typedef unsigned long long t_uint64;

/* The 48-bit word needs 64 bits; there is no 32-bit build. */
typedef t_uint64 t_value;

#include "image.h" /* names the types above */

#if !defined(PATH_MAX)
#define PATH_MAX 512
#endif
#if (PATH_MAX >= 128)
#define CBUFSIZE (128 + PATH_MAX)
#else
#define CBUFSIZE 256
#endif

/* Status codes.  0 is ok, 1..SCPE_BASE-1 are the machine's STOP_ codes in
 * besm6_defs.h, and these are the framework's. */
#define SCPE_OK      0
#define SCPE_BASE    64
#define SCPE_UNATT   (SCPE_BASE + 0)  /* not attached */
#define SCPE_IOERR   (SCPE_BASE + 1)  /* I/O error */
#define SCPE_FMT     (SCPE_BASE + 2)  /* loader format */
#define SCPE_NOATT   (SCPE_BASE + 3)  /* not attachable */
#define SCPE_OPENERR (SCPE_BASE + 4)  /* open error */
#define SCPE_MEM     (SCPE_BASE + 5)  /* alloc error */
#define SCPE_ARG     (SCPE_BASE + 6)  /* argument error */
#define SCPE_RO      (SCPE_BASE + 7)  /* read only */
#define SCPE_STOP    (SCPE_BASE + 8)  /* stopped */
#define SCPE_TTIERR  (SCPE_BASE + 9)  /* console error */
#define SCPE_NOFNC   (SCPE_BASE + 10) /* not implemented */
#define SCPE_NORO    (SCPE_BASE + 11) /* read only not ok */
#define SCPE_NXPAR   (SCPE_BASE + 12) /* no such parameter */
#define SCPE_IERR    (SCPE_BASE + 13) /* internal error */
#define SCPE_2MARG   (SCPE_BASE + 14) /* too many arguments */
#define SCPE_MAX_ERR (SCPE_BASE + 14)

/* Rides on a status to say the message has already been printed. */
#define SCPE_NOMESSAGE 0x40000000

#define SWMASK(x) (1u << (((int)(x)) - ((int)'A'))) /* ATTACH's -n and -e */

/* End of the queue: not NULL, so NULL means "not queued", and not a pointer. */
#define QUEUE_LIST_END ((UNIT *)1)

#define NOQUEUE_WAIT 1000000 /* sim_interval with nothing queued */

typedef struct DEVICE DEVICE;
typedef struct UNIT UNIT;

struct DEVICE {
    const char *name;
    UNIT *units;
    uint32 numunits;
    t_stat (*reset)(DEVICE *dp);
    t_stat (*detach)(UNIT *up);
    uint32 dctrl; /* trace control */
};

struct UNIT {
    UNIT *next;                 /* next on the event queue */
    t_stat (*action)(UNIT *up); /* what its event does */
    char *filename;
    Image *image; /* the attached disk or drum */
    int32 time;   /* timeout, relative to the entry in front */
    uint32 flags;
    char *uname;
    DEVICE *dptr; /* backpointer */
    int32 wait;
};

#define UNIT_V_UF 16 /* where a device's own flags begin */

#define UNIT_ATTABLE 0000001
#define UNIT_RO      0000002
#define UNIT_ATT     0000020
#define UNIT_ROABLE  0001000

/* The event queue: a UNIT with a timeout in instructions, relative to the entry
 * in front of it; sim_interval counts down to the head.  Upstream had a second
 * queue hung off a calibrated clock in another file and mutually recursive with
 * this one, so that several simulators' clocks could be kept honest at once.
 * There is one machine here and one clock in it. */
extern int32 sim_interval;
extern UNIT *sim_clock_queue;
extern volatile t_bool stop_cpu; /* ^E, or a signal */
extern int32 sim_switches;

t_stat sim_process_event(void);
t_stat sim_activate(UNIT *uptr, int32 instructions);
t_stat sim_activate_after(UNIT *uptr, double usecs);
t_stat sim_cancel(UNIT *uptr);
t_bool sim_is_active(UNIT *uptr);

/* The model's clock: how many instructions a microsecond is, kept near the real
 * rate by the 250 Hz clock reporting in once a second. */
t_stat sim_register_clock_unit(UNIT *uptr);
int32 sim_rtcn_init(int32 instructions_per_tick);
int32 sim_rtcn_calb(uint32 ticks_per_second);

/* Runs `uptr' with the clock's next tick, `n' ticks from now. */
t_stat sim_clock_coschedule(UNIT *uptr, int32 n);

time_t sim_get_time(void); /* the wall clock, for the guest's date */

t_stat attach_unit(UNIT *uptr, const char *cptr);
t_stat detach_unit(UNIT *uptr);
const char *sim_uname(UNIT *uptr);
DEVICE *find_dev_from_unit(UNIT *uptr);
char *sim_basename(const char *filepath);

#if defined(__GNUC__)
#define GCC_FMT_ATTR(n, m) __attribute__((format(__printf__, n, m)))
#else
#define GCC_FMT_ATTR(n, m)
#endif

const char *sim_error_text(t_stat stat);
void sim_printf(const char *fmt, ...) GCC_FMT_ATTR(1, 2);
t_stat sim_messagef(t_stat stat, const char *fmt, ...) GCC_FMT_ATTR(2, 3);

/*
 * A transfer the machine asked for and the driver has not performed yet.
 * Upstream did the host fread() inside the `увв' instruction and deferred only
 * the completion interrupt; on Braam a read is a co_await and the instruction
 * loop must not contain one.  The guest waits for its ГРП interrupt either way,
 * so deferring the data with it is the more faithful model too.
 *
 * One is enough: the driver runs before the next instruction.
 */
typedef struct {
    UNIT *unit; /* NULL when nothing is pending */
    t_stat (*serve)(UNIT *u);
    int32 delay; /* model time until the completion event */
} IoRequest;

extern IoRequest io_request;

void io_post(UNIT *u, t_stat (*serve)(UNIT *u), int32 delay);
t_stat io_service(void);

/*
 * The driver.  cpu_burst() runs instructions until it has something for its
 * caller to do and says which; the caller is a C main() here and a coroutine on
 * Braam, and that is the whole of the difference.
 *
 * Nothing below cpu_burst() may block: a read, a write and a sleep are
 * coroutines there, and a coroutine cannot be entered from a plain function.
 * Making the loop itself one is worse -- a co_await is a call and not a tail
 * call, so awaiting without suspending grows the native stack until it traps.
 */
enum {
    REASON_IO    = -1, /* a transfer waits: io_service() */
    REASON_YIELD = -2, /* the burst is up: flush, take keys, let the world turn */
};
/* Anything >= 0 is a stop code and the machine has halted. */

t_stat machine_init(void);
void sim_run_begin(void);
void sim_run_end(void);
int machine_exit(t_stat stat);

/* Long enough to amortise what the caller does between bursts, short enough to
 * keep a console responsive. */
#define BURST_INSTRUCTIONS 100000

t_stat cpu_burst(void);

extern DEVICE *sim_devices[];
extern const char *sim_stop_messages[SCPE_BASE];

#endif /* BESM6_MACHINE_H */
