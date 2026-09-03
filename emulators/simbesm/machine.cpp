// The machine: messages, devices, the event queue, the clock and the driver.
//
// Copyright (c) 1993-2022, Robert M Supnik and the Open SIMH contributors
// Copyright (c) 2026, Serge Vakulenko
#include "besm6_defs.h"

int32_t sim_interval;
int32_t sim_switches;
UNIT *sim_clock_queue = QUEUE_LIST_END;
volatile bool stop_cpu;

static bool sim_is_running;
static int exit_status = EXIT_SUCCESS;

static status_t reset_all(void);
static void detach_all(void);

// ------------------------------------------------------------------ messages

static const char *const scp_errors[1 + SCPE_MAX_ERR - SCPE_BASE] = {
    "Unit not attached",      "I/O error",
    "Format error",           "Unit not attachable",
    "File open error",        "Memory exhausted",
    "Invalid argument",       "Read only operation not allowed",
    "Simulation stopped",     "Console terminal setup error",
    "Command not allowed",    "Read only operation not allowed",
    "Non-existent parameter", "Internal error",
    "Too many arguments",
};

const char *sim_error_text(status_t stat)
{
    static char msgbuf[64];

    stat &= ~SCPE_NOMESSAGE;
    if (stat == SCPE_OK)
        return "No Error";
    if ((stat >= SCPE_BASE) && (stat <= SCPE_MAX_ERR))
        return scp_errors[stat - SCPE_BASE];
    snprintf(msgbuf, sizeof(msgbuf), "Error %d", stat);
    return msgbuf;
}

static void sim_emit(const char *buf)
{
    if (sim_is_running) {
        const char *c, *remnant = buf;

        while ((c = strchr(remnant, '\n'))) {
            sink_write(sim_con, remnant, (int)(c - remnant));
            sink_puts(sim_con, ((c != buf) && (*(c - 1) != '\r')) ? "\r\n" : "\n");
            remnant = c + 1;
        }
        sink_puts(sim_con, remnant);
    } else
        sink_puts(sim_con, buf);
}

void sim_printf(const char *fmt, ...)
{
    char buf[CBUFSIZE];
    va_list arglist;

    va_start(arglist, fmt);
    vsnprintf(buf, sizeof(buf), fmt, arglist);
    va_end(arglist);

    sim_emit(buf);
    sink_puts(sim_deb, buf);
}

status_t sim_messagef(status_t stat, const char *fmt, ...)
{
    char buf[CBUFSIZE];
    va_list arglist;
    bool inhibit_message = (stat & SCPE_NOMESSAGE) != 0;
    bool newline_prefix  = (*fmt == '\n');
    int prefix_len;

    if ((stat == SCPE_OK) && (sim_switches & SWMASK('Q')))
        return stat;
    if (newline_prefix)
        ++fmt;
    prefix_len = snprintf(buf, sizeof(buf),
                          "%s%%SIM-%s: ", newline_prefix ? (sim_is_running ? "\r\n" : "\n") : "",
                          (stat == SCPE_OK) ? "INFO" : "ERROR");
    va_start(arglist, fmt);
    vsnprintf(buf + prefix_len, sizeof(buf) - prefix_len, fmt, arglist);
    va_end(arglist);

    if (!inhibit_message)
        sim_emit(buf);
    // Always display messages in debug output
    sink_puts(sim_deb, buf);

    return stat | ((stat != SCPE_OK) ? SCPE_NOMESSAGE : 0);
}

char *sim_basename(const char *filepath)
{
    const char *name = strrchr(filepath, '/');
    const char *dot;
    size_t len;
    char *result;

    name   = name ? name + 1 : filepath;
    dot    = strrchr(name, '.');
    len    = dot ? (size_t)(dot - name) : strlen(name);
    result = (char *)malloc(len + 1);
    if (result == NULL)
        return NULL;
    memcpy(result, name, len);
    result[len] = '\0';
    return result;
}

// -------------------------------------------------------- units and devices

DEVICE *find_dev_from_unit(UNIT *uptr)
{
    DEVICE *dptr;
    uint32_t i, j;

    if (uptr == NULL)
        return NULL;
    if (uptr->dptr)
        return uptr->dptr;
    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        for (j = 0; j < dptr->numunits; j++) {
            if (uptr == (dptr->units + j)) {
                uptr->dptr = dptr;
                return dptr;
            }
        }
    }
    return NULL;
}

const char *sim_uname(UNIT *uptr)
{
    DEVICE *d;
    char uname[CBUFSIZE];

    if (!uptr)
        return "";
    if (uptr->uname)
        return uptr->uname;
    d = find_dev_from_unit(uptr);
    if (!d)
        return "";
    if (d->numunits == 1)
        snprintf(uname, sizeof(uname), "%s", d->name);
    else
        snprintf(uname, sizeof(uname), "%s%d", d->name, (int)(uptr - d->units));
    return uptr->uname = strcpy((char *)malloc(1 + strlen(uname)), uname);
}

static status_t attach_err(UNIT *uptr, status_t stat)
{
    free(uptr->filename);
    uptr->filename = NULL;
    return stat;
}

status_t attach_unit(UNIT *uptr, const char *cptr)
{
    int create    = (sim_switches & SWMASK('N')) != 0;
    int must_have = (sim_switches & SWMASK('E')) != 0;
    int how       = IMG_OPENED;
    int why       = SCPE_OK;

    if (!(uptr->flags & UNIT_ATTABLE)) // not attachable?
        return SCPE_NOATT;
    if (find_dev_from_unit(uptr) == NULL)
        return SCPE_NOATT;
    uptr->filename = (char *)calloc(CBUFSIZE, sizeof(char)); // alloc name buf
    if (uptr->filename == NULL)
        return SCPE_MEM;
    strlcpy(uptr->filename, cptr, CBUFSIZE); // save name

    uptr->image = img_open(cptr, create, must_have, (uptr->flags & UNIT_ROABLE) != 0, &how, &why);
    if (uptr->image == NULL) {
        if (why == SCPE_NORO)
            return sim_messagef(attach_err(uptr, why), "%s: Read Only operation not allowed\n",
                                sim_uname(uptr));
        return sim_messagef(attach_err(uptr, why), "%s: Can't open '%s': %s\n", sim_uname(uptr),
                            cptr, strerror(errno));
    }
    if (how == IMG_RDONLY) {
        uptr->flags |= UNIT_RO;
        sim_messagef(SCPE_OK, "%s: unit is read only\n", sim_uname(uptr));
    } else if (how == IMG_CREATED) {
        if (create)
            sim_messagef(SCPE_OK, "%s: creating new file: %s\n", sim_uname(uptr), cptr);
        else
            sim_messagef(SCPE_OK, "%s: creating new file\n", sim_uname(uptr));
    }

    uptr->flags = uptr->flags | UNIT_ATT;
    return SCPE_OK;
}

status_t detach_unit(UNIT *uptr)
{
    if (uptr == NULL)
        return SCPE_IERR;
    if (!(uptr->flags & UNIT_ATTABLE)) // attachable?
        return SCPE_NOATT;
    if (!(uptr->flags & UNIT_ATT)) // not attached?
        return SCPE_UNATT;
    if (find_dev_from_unit(uptr) == NULL)
        return SCPE_OK;
    uptr->flags = uptr->flags & ~(UNIT_ATT | ((uptr->flags & UNIT_ROABLE) ? UNIT_RO : 0));
    free(uptr->filename);
    uptr->filename = NULL;
    if (uptr->image) { // Only close an open image
        int bad     = img_close(uptr->image);
        uptr->image = NULL;
        if (bad)
            return SCPE_IOERR;
    }
    return SCPE_OK;
}

static void detach_all(void)
{
    uint32_t i, j;
    DEVICE *dptr;
    UNIT *uptr;

    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) { // loop thru dev
        for (j = 0; j < dptr->numunits; j++) {          // loop thru units
            uptr = (dptr->units) + j;
            if ((uptr->flags & UNIT_ATT) ||      // attached?
                (dptr->detach &&                 // or a device routine,
                 !(uptr->flags & UNIT_ATTABLE))) // !attachable?
                (dptr->detach != NULL) ? dptr->detach(uptr) : detach_unit(uptr);
        }
    }
}

static status_t reset_all(void)
{
    DEVICE *dptr;
    uint32_t i;
    status_t reason;

    for (i = 0; (dptr = sim_devices[i]) != NULL; i++) {
        if (dptr->reset == NULL)
            continue;
        reason = dptr->reset(dptr);
        if (reason != SCPE_OK) {
            sink_printf(sim_con,
                        "Fatal simulator initialization error\n"
                        "Device %s initial reset call returned: %s\n",
                        dptr->name, sim_error_text(reason));
            return reason;
        }
    }
    return SCPE_OK;
}

// -------------------------------------------------------------- event queue

// Fire whatever is due.  Upstream also backed time up when an asynchronous
// timer had overshot; sim_interval only counts down by one here, so it reaches
// zero exactly and there is nothing to catch up with.
status_t sim_process_event(void)
{
    UNIT *uptr;
    status_t reason = SCPE_OK;

    if (stop_cpu) {
        stop_cpu = false;
        return SCPE_STOP;
    }
    if (sim_interval > 0)
        return SCPE_OK;
    if (sim_clock_queue == QUEUE_LIST_END) {
        sim_interval = NOQUEUE_WAIT;
        return SCPE_OK;
    }
    do {
        uptr            = sim_clock_queue;
        sim_clock_queue = uptr->next;
        uptr->next      = NULL;
        uptr->time      = 0;
        sim_interval = (sim_clock_queue != QUEUE_LIST_END) ? sim_clock_queue->time : NOQUEUE_WAIT;

        reason = uptr->action ? uptr->action(uptr) : SCPE_OK;
        if (reason != SCPE_OK && reason >= SCPE_BASE && reason != SCPE_STOP)
            sim_messagef(reason, "\nUnexpected error from %s: %s\n", sim_uname(uptr),
                         sim_error_text(reason));
    } while (reason == SCPE_OK && sim_interval <= 0 && sim_clock_queue != QUEUE_LIST_END &&
             !stop_cpu);

    if (sim_clock_queue == QUEUE_LIST_END)
        sim_interval = NOQUEUE_WAIT;
    if (reason == SCPE_OK && stop_cpu) {
        stop_cpu = false;
        reason   = SCPE_STOP;
    }
    return reason;
}

status_t sim_activate(UNIT *uptr, int32_t event_time)
{
    UNIT *cptr, *prvptr;
    int32_t accum;

    if (sim_is_active(uptr)) // already active?
        return SCPE_OK;

    prvptr = NULL;
    accum  = 0;
    for (cptr = sim_clock_queue; cptr != QUEUE_LIST_END; cptr = cptr->next) {
        if (event_time < (accum + cptr->time))
            break;
        accum  = accum + cptr->time;
        prvptr = cptr;
    }
    if (prvptr == NULL) { // insert at head
        cptr = uptr->next = sim_clock_queue;
        sim_clock_queue   = uptr;
    } else {
        cptr = uptr->next = prvptr->next; // insert at prvptr
        prvptr->next      = uptr;
    }
    uptr->time = event_time - accum;
    if (cptr != QUEUE_LIST_END)
        cptr->time = cptr->time - uptr->time;
    sim_interval = sim_clock_queue->time;
    return SCPE_OK;
}

status_t sim_cancel(UNIT *uptr)
{
    UNIT *cptr, *nptr;

    if (sim_clock_queue == QUEUE_LIST_END)
        return SCPE_OK;
    if (!sim_is_active(uptr))
        return SCPE_OK;
    nptr = QUEUE_LIST_END;

    if (sim_clock_queue == uptr) {
        nptr = sim_clock_queue = uptr->next;
        uptr->next             = NULL; // hygiene
    } else {
        for (cptr = sim_clock_queue; cptr != QUEUE_LIST_END; cptr = cptr->next) {
            if (cptr->next == uptr) {
                nptr = cptr->next = uptr->next;
                uptr->next        = NULL; // hygiene
                break;                    // end queue scan
            }
        }
    }
    if (nptr != QUEUE_LIST_END)
        nptr->time += (uptr->next) ? 0 : uptr->time;
    if (!uptr->next)
        uptr->time = 0;
    if (sim_clock_queue != QUEUE_LIST_END)
        sim_interval = sim_clock_queue->time;
    else
        sim_interval = NOQUEUE_WAIT;
    return SCPE_OK;
}

bool sim_is_active(UNIT *uptr)
{
    return uptr->next != nullptr;
}

// --------------------------------------------------------------- the clock

// The calibration's state.  CLK_TPS_GUESS is the rate assumed before the first
// calibration; SIM_TMAX is the widest gap the loop will slew over.
#define CLK_TPS_GUESS   100
#define SIM_INITIAL_IPS 500000
#define SIM_TMAX        500

static uint32_t rtc_ticks;   // ticks this second
static uint32_t rtc_hz;      // tick rate
static uint32_t rtc_last_hz; // prior tick rate
static uint32_t rtc_rtime;   // real time, ms
static uint32_t rtc_vtime;   // virtual time, ms
static uint32_t rtc_nxintv;  // next interval
static int32_t rtc_based;    // base delay
static int32_t rtc_currd;    // current delay

static double inst_per_sec(void)
{
    double ips = (double)rtc_currd * rtc_hz;

    if (ips == 0.0) // rate not calibrated yet?
        ips = (double)rtc_currd * CLK_TPS_GUESS;
    if (ips == 0.0)
        ips = SIM_INITIAL_IPS;
    return ips;
}

int32_t sim_rtcn_init(int32_t time)
{
    if (time == 0)
        time = 1;
    // If we'd previously succeeded in calibrating a tick value, then use that
    // delay as a better default to setup when we're re-initialized.
    if (rtc_currd)
        time = rtc_currd;
    rtc_rtime   = sim_now_ms();
    rtc_vtime   = rtc_rtime;
    rtc_nxintv  = 1000;
    rtc_ticks   = 0;
    rtc_last_hz = rtc_hz;
    rtc_hz      = 0;
    rtc_based   = time;
    rtc_currd   = time;
    return time;
}

int32_t sim_rtcn_calb(uint32_t ticksper)
{
    uint32_t new_rtime, delta_rtime;
    int32_t delta_vtime;

    if (rtc_hz != ticksper) { // changing tick rate?
        if ((rtc_last_hz != 0) && (rtc_last_hz != ticksper) && (ticksper != 0))
            rtc_currd = (int32_t)(inst_per_sec() / ticksper);
        rtc_last_hz = rtc_hz;
        rtc_hz      = ticksper;
    }
    if (ticksper == 0) // running?
        return 10000;
    rtc_ticks += 1;           // count ticks
    if (rtc_ticks < ticksper) // 1 sec yet?
        return rtc_currd;
    rtc_ticks = 0;            // reset ticks
    new_rtime = sim_now_ms(); // wall time
    if (new_rtime < rtc_rtime) {
        // Time running backwards: sim_now_ms() wrapped as a uint32_t, which
        // happens roughly every 49 days.  Rebase and skip this calibration.
        rtc_vtime = rtc_rtime = new_rtime;
        rtc_nxintv            = 1000;
        rtc_based             = rtc_currd;
        return rtc_currd;
    }
    delta_rtime = new_rtime - rtc_rtime; // elapsed wtime
    rtc_rtime   = new_rtime;             // adv wall time
    rtc_vtime += 1000;                   // adv sim time
    if (delta_rtime > 30000) {
        // Gap too big: the process was suspended, or the host slept.  Ignore
        // what happened and proceed from here.
        rtc_vtime  = rtc_rtime; // sync virtual and real time
        rtc_nxintv = 1000;      // reset next interval
        rtc_based  = rtc_currd;
        return rtc_currd; // can't calibrate
    }
    // This self regulating algorithm depends directly on the assumption
    // that this routine is called back after processing the number of
    // instructions which was returned the last time it was called.
    if (delta_rtime == 0)                 // gap too small?
        rtc_based = rtc_based * ticksper; // slew wide
    else
        rtc_based = (int32_t)(((double)rtc_based * (double)rtc_nxintv) /
                              ((double)delta_rtime)); // new base rate
    delta_vtime = rtc_vtime - rtc_rtime;              // gap
    if (delta_vtime > SIM_TMAX)                       // limit gap
        delta_vtime = SIM_TMAX;
    else {
        if (delta_vtime < -SIM_TMAX)
            delta_vtime = -SIM_TMAX;
    }
    rtc_nxintv = 1000 + delta_vtime;                                           // next wtime
    rtc_currd  = (int32_t)(((double)rtc_based * (double)rtc_nxintv) / 1000.0); // next delay
    if (rtc_based <= 0) // never negative or zero!
        rtc_based = 1;
    if (rtc_currd <= 0) // never negative or zero!
        rtc_currd = 1;
    return rtc_currd;
}

// The clock unit and the two calls that schedule against it.  Upstream ran
// these off a second queue with an assist unit standing in for the clock and a
// "coschedule" list behind it; one machine with one clock needs neither.
static UNIT *clock_unit;

status_t sim_register_clock_unit(UNIT *uptr)
{
    clock_unit = uptr;
    return SCPE_OK;
}

// Instructions until `uptr' fires, or -1 when it is not queued.
static int32_t queue_time(UNIT *uptr)
{
    UNIT *cptr;
    int32_t accum = 0;

    for (cptr = sim_clock_queue; cptr != QUEUE_LIST_END; cptr = cptr->next) {
        accum += cptr->time;
        if (cptr == uptr)
            return accum;
    }
    return -1;
}

status_t sim_activate_after(UNIT *uptr, double usecs)
{
    int32_t n = (int32_t)(usecs * inst_per_sec() / 1000000.0);

    return sim_activate(uptr, n > 0 ? n : 1);
}

status_t sim_clock_coschedule(UNIT *uptr, int32_t n)
{
    int32_t tick = rtc_currd > 0 ? rtc_currd : 1;
    int32_t when = clock_unit ? queue_time(clock_unit) : -1;

    if (when < 0)
        when = tick;
    return sim_activate(uptr, when + n * tick);
}

// ------------------------------------------------------------------- idling

int sim_idle_enab = 1;

// A sleep is a whole millisecond at best, so anything sooner is not worth
// sleeping for -- a disk answers in 20 us of model time, and paying a
// millisecond for it would make every transfer cost one.  A sleep also cannot
// be woken early, so IDLE_MAX_MS is the longest a keystroke waits; the head
// event is the CLK_TPS tick, 4 ms away, so the cap rarely binds.
#define IDLE_MIN_MS 1.0
#define IDLE_MAX_MS 20

// Zero means "do not idle": too soon to sleep for, or a frozen sim_now_ms()
// making the rate nonsense.  Rounded rather than truncated, so a tick is slept
// out whole; sleeping a little long is what the calibration corrects for.
uint32_t sim_idle_ms(void)
{
    double ms = sim_interval * 1000.0 / inst_per_sec();

    if (ms < IDLE_MIN_MS)
        return 0;
    if (ms > IDLE_MAX_MS)
        ms = IDLE_MAX_MS;
    return (uint32_t)(ms + 0.5);
}

void sim_idle_skip(uint32_t ms)
{
    double n = ms * inst_per_sec() / 1000.0;

    if (n >= sim_interval) // never past the event the spin was waiting for
        sim_interval = 0;
    else
        sim_interval -= (int32_t)n;
}

// ------------------------------------------------------- deferred transfers

IoRequest io_request;

void io_post(UNIT *u, int write, int32_t delay, int *fail, int fail_mask)
{
    io_request.unit      = u;
    io_request.write     = write;
    io_request.nrun      = 0;
    io_request.delay     = delay;
    io_request.fail      = fail;
    io_request.fail_mask = fail_mask;
}

void io_run(uint32_t off, value_t *mem, int n)
{
    IoRun *r;

    if (io_request.nrun >= (int)(sizeof(io_request.run) / sizeof(io_request.run[0])))
        return;
    r      = &io_request.run[io_request.nrun++];
    r->off = off;
    r->mem = mem;
    r->n   = n;
}

// ---------------------------------------------------------------- the driver

status_t machine_init(void)
{
    status_t r;

    sink_init();
    sim_switches    = 0;
    stop_cpu        = false;
    sim_interval    = 0;
    sim_clock_queue = QUEUE_LIST_END;
    sim_is_running  = false;

    if ((r = con_init()) != SCPE_OK) {
        sim_printf("Fatal terminal initialization error\n%s\n", sim_error_text(r));
        exit_status = EXIT_FAILURE;
        return r;
    }
    if ((r = reset_all()) != SCPE_OK) {
        exit_status = EXIT_FAILURE;
        return r;
    }
    sim_debug_from_env();
    if (getenv("BESM6_NOIDLE"))
        sim_idle_enab = 0;
    return SCPE_OK;
}

// Brackets the run.  sim_is_running is what makes sim_emit() return the
// carriage: a bare \n does not, with the console in raw mode.
void sim_run_begin(void)
{
    sim_is_running = true;
    con_flush(); // whatever the setup said, before the machine starts
}

void sim_run_end(void)
{
    sim_is_running = false;
    con_flush();
}

int machine_exit(status_t stat)
{
    detach_all();
    sim_debug_close();
    con_cooked();
    if (exit_status != EXIT_SUCCESS)
        return exit_status; // startup failed
    // SCPE_STOP is the stop key, and a STOP_ code is the machine halting: both
    // are how a run ends normally.  Only a framework failure is a failure.
    stat &= ~SCPE_NOMESSAGE;
    return (stat < SCPE_BASE || stat == SCPE_STOP) ? EXIT_SUCCESS : EXIT_FAILURE;
}
