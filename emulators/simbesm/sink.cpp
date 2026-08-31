/*
 * Formatting into a Sink, and where the two of them go.
 *
 * Copyright (c) 2026, Serge Vakulenko
 */
#include "besm6_defs.h"

/* The longest line the tracer emits is a register dump, about 120 characters.
 * A longer one is formatted again into a heap block. */
#define SINK_BUF 512

void sink_write(Sink *s, const char *buf, int n)
{
    if (s && n > 0)
        s->put(s, buf, n);
}

void sink_puts(Sink *s, const char *str)
{
    if (s && str)
        sink_write(s, str, (int)strlen(str));
}

void sink_vprintf(Sink *s, const char *fmt, va_list args)
{
    char buf[SINK_BUF];
    va_list copy;
    int n;

    if (!s)
        return;

    va_copy(copy, args);
    n = vsnprintf(buf, sizeof(buf), fmt, copy);
    va_end(copy);
    if (n < 0)
        return;
    if (n < (int)sizeof(buf)) {
        sink_write(s, buf, n);
        return;
    }

    /* Too long for the stack buffer: format it again into one that fits. */
    char *big = (char *)malloc(n + 1);
    if (!big) {
        sink_write(s, buf, sizeof(buf) - 1); /* what did fit */
        return;
    }
    n = vsnprintf(big, n + 1, fmt, args);
    if (n > 0)
        sink_write(s, big, n);
    free(big);
}

void sink_printf(Sink *s, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    sink_vprintf(s, fmt, args);
    va_end(args);
}

/* ---------------------------------------------------------------- backends */

/* The operator's console is the machine's: the same terminal, so the same
 * buffer, or a message from inside an instruction would overtake the guest
 * output in front of it -- and on Braam could not be written at all. */
static void con_sink_put(Sink *s, const char *buf, int n)
{
    int i;

    (void)s;
    for (i = 0; i < n; i++)
        con_put(CON_SCREEN, (unsigned char)buf[i]);
}

static Sink con_sink = { con_sink_put };

Sink *sim_con;
Sink *sim_deb;

/* -------------------------------------------------------------- the switches */

/* The console is bound first, so a startup failure has somewhere to go. */
void sink_init(void)
{
    sim_con = &con_sink;
}

/*
 * BESM6_DEBUG names the trace file, "-" being stderr; BESM6_TRACE is a
 * comma-separated device list, defaulting to "cpu".  With BESM6_DEBUG unset
 * sim_deb stays NULL and every trace site is a branch on it.
 *
 *      BESM6_DEBUG=- BESM6_TRACE=cpu,mmu ./besm6
 */
void sim_debug_from_env(void)
{
    const char *file = getenv("BESM6_DEBUG");
    const char *devs = getenv("BESM6_TRACE");
    DEVICE *dptr;
    uint32_t i;

    if ((file == NULL) || (*file == '\0'))
        return;
    sim_deb = deb_file_open(file);
    if (sim_deb == NULL)
        return;

    if ((devs == NULL) || (*devs == '\0'))
        devs = "cpu";
    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        const char *p = devs;
        size_t n      = strlen(dptr->name);

        while (*p) { /* is dptr->name one of the comma separated words? */
            const char *e = strchr(p, ',');
            size_t len    = e ? (size_t)(e - p) : strlen(p);

            if ((len == n) && (strncasecmp(p, dptr->name, n) == 0)) {
                dptr->dctrl = 0xffffffff;
                break;
            }
            if (!e)
                break;
            p = e + 1;
        }
    }
}

void sim_debug_close(void)
{
    if (sim_deb == NULL)
        return;
    deb_file_close();
    sim_deb = NULL;
}
