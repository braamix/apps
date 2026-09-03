/*
 * The host build's platform: images over stdio, the console over termios, and
 * the trace file.  braam.cpp is the other one.
 *
 * Copyright (c) 2026, Serge Vakulenko
 */
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "besm6_defs.h"

/* =========================================================== the trace file */

typedef struct {
    Sink base;
    FILE *f;
} FileSink;

static void file_put(Sink *s, const char *buf, int n)
{
    FILE *f = ((FileSink *)s)->f;

    fwrite(buf, 1, n, f);
    fflush(f);
}

static FileSink deb_sink = { { file_put }, NULL };

Sink *deb_file_open(const char *path)
{
    if (strcmp(path, "-") == 0)
        deb_sink.f = stderr;
    else if ((deb_sink.f = fopen(path, "w")) == NULL) {
        fprintf(stderr, "Can't open debug file '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    return &deb_sink.base;
}

void deb_file_close(void)
{
    if (deb_sink.f && deb_sink.f != stderr)
        fclose(deb_sink.f);
    deb_sink.f = NULL;
}

uint32_t sim_now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint32_t)((int64_t)(ts.tv_sec * 1000) + (int64_t)((ts.tv_nsec + 500000) / 1000000));
}

void sim_get_time(SimTime *t)
{
    time_t now   = time(NULL);
    struct tm *d = localtime(&now);

    t->year = d->tm_year;
    t->mon  = d->tm_mon;
    t->mday = d->tm_mday;
    t->hour = d->tm_hour;
    t->min  = d->tm_min;
}

/* ================================================================== images */

struct Image {
    FILE *f;
    int err;
};

Image *img_open(const char *path, int create, int must_exist, int roable, int *how, int *why)
{
    Image *m;
    FILE *f;

    *how = create ? IMG_CREATED : IMG_OPENED;
    *why = SCPE_OK;

    if (create) {
        f = fopen(path, "wb+");
        if (!f) {
            *why = SCPE_OPENERR;
            return NULL;
        }
    } else {
        f = fopen(path, "rb+");
        if (!f) {
            if ((errno == EROFS) || (errno == EACCES) || (errno == EPERM)) {
                if (!roable) {
                    *why = SCPE_NORO;
                    return NULL;
                }
                f = fopen(path, "rb");
                if (!f) {
                    *why = SCPE_OPENERR;
                    return NULL;
                }
                *how = IMG_RDONLY;
            } else if (must_exist) {
                *why = SCPE_OPENERR;
                return NULL;
            } else {
                f = fopen(path, "wb+");
                if (!f) {
                    *why = SCPE_OPENERR;
                    return NULL;
                }
                *how = IMG_CREATED;
            }
        }
    }

    m = (Image *)calloc(1, sizeof(*m));
    if (!m) {
        fclose(f);
        *why = SCPE_MEM;
        return NULL;
    }
    m->f = f;
    return m;
}

int img_close(Image *m)
{
    int bad;

    if (!m)
        return 0;
    bad = m->err || (fclose(m->f) == EOF);
    free(m);
    return bad;
}

int img_read(Image *m, uint32_t off, value_t *dst, int n)
{
    size_t got;

    if (fseek(m->f, (long)off * 8, SEEK_SET) != 0)
        return 0;
    got = fread(dst, 8, n, m->f);
    if (ferror(m->f))
        m->err = 1;
    return (int)got;
}

int img_write(Image *m, uint32_t off, const value_t *src, int n)
{
    size_t put;

    if (fseek(m->f, (long)off * 8, SEEK_SET) != 0)
        return 0;
    put = fwrite(src, 8, n, m->f);
    if (ferror(m->f))
        m->err = 1;
    return (int)put;
}

int img_append(Image *m, const value_t *src, int n)
{
    size_t put = fwrite(src, 8, n, m->f);

    if (ferror(m->f))
        m->err = 1;
    return (int)put;
}

int img_error(Image *m)
{
    return m ? m->err : 0;
}

void img_remove(const char *path)
{
    remove(path);
}

int img_slurp(const char *path, Blob *b)
{
    FILE *f = fopen(path, "rb");
    long n;
    unsigned char *p;

    b->base = NULL;
    b->len = b->pos = 0;
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    p = (unsigned char *)malloc((size_t)n + 1);
    if (!p) {
        fclose(f);
        return -1;
    }
    b->len  = fread(p, 1, (size_t)n, f);
    b->base = p;
    fclose(f);
    return 0;
}

double sim_strtod(const char *s, char **end)
{
    return strtod(s, end);
}

void blob_free(Blob *b)
{
    free((void *)b->base);
    b->base = NULL;
    b->len = b->pos = 0;
}

/* ================================================================ consoles */

static struct termios cmdtty, runtty;
static int cmdfl, runfl; /* TTY flags */

/* Stops the machine: termios VINTR takes it, so it arrives as SIGINT.  Braam
 * has no VINTR and binds Alt+Q instead. */
static const int32_t con_stop_char = 005; /* ^E */

static bool con_isatty(void)
{
    static int answer = -1;

    if (answer == -1)
        answer = isatty(0);
    return answer != 0;
}

/* One terminal here: a second screen is a browser tab's. */
int con_second(void)
{
    return 0;
}

/* The console is stdout; there is nowhere else to write on the host. */
void con_flush(void)
{
    const char *buf;
    int n = con_take(CON_SCREEN, &buf);

    if (n > 0 && write(1, buf, n) != n) {
        /* nothing useful to do about a full or closed stdout */
    }
}

/* Whatever has been typed, into the ring.  Upstream read one byte where the
 * machine asked for one; the ring is what the two builds share. */
static void con_poll(void)
{
    unsigned char buf[64];
    ssize_t n;
    int i;

    if (!con_isatty())
        return;
    n = read(0, buf, sizeof(buf));
    for (i = 0; i < n; i++)
        con_feed(CON_SCREEN, buf[i]);
}

status_t con_init(void)
{
    cmdfl = fcntl(0, F_GETFL, 0); /* get old flags  and status */
    /*
     * make sure systems with broken termios (that don't honor
     * VMIN=0 and VTIME=0) actually implement non blocking reads.
     * This will have no negative effect on other systems since
     * this is turned on and off depending on whether simulation
     * is running or not.
     */
    runfl = cmdfl | O_NONBLOCK;
    if (!con_isatty()) /* skip if !tty */
        return SCPE_OK;
    if (tcgetattr(0, &cmdtty) < 0) /* get old flags */
        return SCPE_TTIERR;
    runtty              = cmdtty;
    runtty.c_lflag      = runtty.c_lflag & ~(ECHO | ICANON); /* no echo or edit */
    runtty.c_oflag      = runtty.c_oflag & ~OPOST;           /* no output edit */
    runtty.c_iflag      = runtty.c_iflag & ~ICRNL;           /* no cr conversion */
    runtty.c_iflag      = runtty.c_iflag & ~IGNCR;           /* don't ignore cr */
    runtty.c_iflag      = runtty.c_iflag & ~IXANY;           /* don't restart after stop */
    runtty.c_iflag      = runtty.c_iflag & ~IMAXBEL; /* don't ring bell on input queue full */
    runtty.c_lflag      = runtty.c_lflag & ~PENDIN;  /* don't retype pending input (state) */
    runtty.c_lflag      = runtty.c_lflag | ECHOK;    /* echo NL after line kill */
    runtty.c_cc[VINTR]  = con_stop_char;             /* interrupt */
    runtty.c_cc[VQUIT]  = 0;                         /* no quit */
    runtty.c_cc[VERASE] = 0;
    runtty.c_cc[VKILL]  = 0;
    runtty.c_cc[VEOF]   = 0;
    runtty.c_cc[VEOL]   = 0;
    runtty.c_cc[VSTART] = 0; /* no host sync */
    runtty.c_cc[VSUSP]  = 0;
    runtty.c_cc[VSTOP]  = 0;
#if defined(VREPRINT)
    runtty.c_cc[VREPRINT] = 0; /* no specials */
#endif
#if defined(VDISCARD)
    runtty.c_cc[VDISCARD] = 0;
#endif
#if defined(VWERASE)
    runtty.c_cc[VWERASE] = 0;
#endif
#if defined(VLNEXT)
    runtty.c_cc[VLNEXT] = 0;
#endif
    runtty.c_cc[VMIN]  = 0; /* no waiting */
    runtty.c_cc[VTIME] = 0;
#if defined(VDSUSP)
    runtty.c_cc[VDSUSP] = 0;
#endif
#if defined(VSTATUS)
    runtty.c_cc[VSTATUS] = 0;
#endif
    return SCPE_OK;
}

/* The stop character arrives as SIGINT, VINTR having taken it. */
static void int_handler(int sig)
{
    (void)sig;
    stop_cpu     = true;
    sim_interval = 0;
}

status_t con_raw(void)
{
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);
    if (!con_isatty()) /* skip if !tty */
        return SCPE_OK;
    (void)fcntl(0, F_SETFL, runfl);     /* non-block mode */
    runtty.c_cc[VINTR] = con_stop_char; /* in case changed */
    if (tcsetattr(0, TCSAFLUSH, &runtty) < 0)
        return SCPE_TTIERR;
    return SCPE_OK;
}

void con_cooked(void)
{
    con_flush();
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    if (!con_isatty()) /* skip if !tty */
        return;
    (void)fcntl(0, F_SETFL, cmdfl); /* block mode */
    (void)tcsetattr(0, TCSAFLUSH, &cmdtty);
}

/* ==================================================== deferred transfers */

/* The runs the machine asked for, performed here because on Braam this is
 * where a read may happen (machine.h). */
status_t io_service(void)
{
    IoRequest *q = &io_request;
    UNIT *u      = q->unit;
    int i;

    if (!u)
        return SCPE_OK;
    q->unit = NULL;

    for (i = 0; i < q->nrun; i++) {
        IoRun *r = &q->run[i];
        int got  = q->write ? img_write(u->image, r->off, r->mem, r->n)
                            : img_read(u->image, r->off, r->mem, r->n);
        if (got != r->n) {
            /* A zone the image was never written that far into. */
            if (q->fail)
                *q->fail |= q->fail_mask;
            break;
        }
    }
    /* A failed transfer halts the machine, which is where upstream's longjmp
     * to cpu_halt landed for an SCPE_ code too. */
    if (img_error(u->image))
        return SCPE_IOERR;

    sim_activate(u, q->delay);
    return SCPE_OK;
}

/* =================================================== the entry point */

/* The one sleep in the native build, and it is the idle path's. */
static void ms_sleep(uint32_t ms)
{
    struct timespec ts;

    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/*
 * The driver loop: what a Braam entry point will be a coroutine of.  Everything
 * that blocks is here and nothing below cpu_burst() may (machine.h).
 */
static status_t run_machine(void)
{
    status_t r;

    for (;;) {
        r = cpu_burst();

        if (r == REASON_IO) {
            r = io_service();
            if (r != SCPE_OK)
                return r;
            continue;
        }
        if (r == REASON_YIELD) {
            con_flush();
            con_poll();
            if (stop_cpu) {
                stop_cpu = false;
                return SCPE_STOP;
            }
            continue;
        }
        if (r == REASON_IDLE) {
            /* The guest is spinning until a queued event: sleep the time those
             * instructions would have taken and charge it to them (machine.h). */
            uint32_t ms;

            con_flush();
            con_poll();
            if (stop_cpu) {
                stop_cpu = false;
                return SCPE_STOP;
            }
            ms = sim_idle_ms();
            if (ms) {
                ms_sleep(ms);
                sim_idle_skip(ms);
            }
            continue;
        }
        return r; /* a stop code */
    }
}

int main(void)
{
    Blob kernel = { NULL, 0, 0 };
    status_t r;

    r = machine_init();
    if (r != SCPE_OK)
        return machine_exit(r);
    sink_puts(sim_con, "\nBESM-6 Simulator v" SIMBESM_VERSION "\n");

    if (img_slurp("unix", &kernel) != 0) {
        sim_printf("Cannot open 'unix'\n");
        return machine_exit(SCPE_OPENERR);
    }
    r = besm6_boot_unix(&kernel);
    blob_free(&kernel);
    if (r != SCPE_OK) {
        sim_printf("%s\n", sim_error_text(r));
        return machine_exit(r);
    }

    con_raw();
    sim_run_begin();
    r = run_machine();
    sim_run_end();
    sink_printf(sim_con, "\n%s\n", (r >= SCPE_BASE) ? sim_error_text(r) : sim_stop_messages[r]);

    return machine_exit(r);
}
