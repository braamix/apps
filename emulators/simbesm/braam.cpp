/*
 * The Braam platform: images over descriptors, the console over the screen, the
 * trace file, and proc_main.  host.cpp is the other one.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Everything that blocks is here, and only here.  The machine below runs as
 * plain C++ and hands the driver what it needs done (machine.h).
 */
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/usage.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/key.h"
#include "kernel/text.h"
#include "fs/path.h"
#include "math/ftoa.h"

#include "besm6_defs.h"

/* ============================================================== the images
 *
 * Opening a file is a co_await and attach_unit() is not one, so the images are
 * opened before the machine is configured and img_open() finds them by path.
 * Four is the whole of it: two packs and two drums.
 */

#define IMG_MAX 4

struct Image {
    char path[64];
    i32 fd;
    int err;
    int used;
};

static Image images[IMG_MAX];

static Image *img_find(const char *path)
{
    for (int i = 0; i < IMG_MAX; i++)
        if (images[i].used && strcmp(images[i].path, path) == 0)
            return &images[i];
    return nullptr;
}

/* The platform's half of an attach: opens the image and remembers it. */
static Task<Result<void>> img_prepare(Str dir, const char *name, bool create)
{
    Image *m = nullptr;

    for (int i = 0; i < IMG_MAX; i++)
        if (!images[i].used) {
            m = &images[i];
            break;
        }
    if (!m)
        co_return Err(Error::NoMemory);

    Buf<256> p;
    p.put(dir).put('/').put(Str(name, strlen(name)));

    u32 flags = SYS_O_READ | SYS_O_WRITE | SYS_O_CREATE;
    if (create)
        flags |= SYS_O_TRUNC;

    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(p.str(), flags))
        fd = co_await t;
    if (fd.is_err())
        co_return Err(fd.error());

    m->fd   = fd.value();
    m->err  = 0;
    m->used = 1;
    usize n = strlen(name);
    if (n >= sizeof(m->path))
        n = sizeof(m->path) - 1;
    memcpy(m->path, name, n);
    m->path[n] = 0;
    co_return {};
}

Image *img_open(const char *path, int create, int must_exist, int roable, int *how, int *why)
{
    (void)create;
    (void)must_exist;
    (void)roable;
    Image *m = img_find(path);

    *how = IMG_OPENED;
    *why = SCPE_OK;
    if (!m) {
        *why = SCPE_OPENERR;
        return nullptr;
    }
    return m;
}

int img_close(Image *m)
{
    return m ? m->err : 0;
}

/* The transfers themselves are the driver's; nothing here moves data. */
int img_read(Image *, uint32, t_value *, int)
{
    return 0;
}

int img_write(Image *, uint32, const t_value *, int)
{
    return 0;
}

int img_error(Image *m)
{
    return m ? m->err : 0;
}

void img_remove(const char *)
{
}

/* The formatter appends; nothing formats a disk on Braam, where the packs
 * arrive already formatted from the package. */
int img_append(Image *, const t_value *, int)
{
    return 0;
}

double sim_strtod(const char *s, char **end)
{
    usize used    = 0;
    Option<f64> v = scan_f64(Str(s, strlen(s)), used);

    if (!v.has_value())
        used = 0;
    if (end)
        *end = (char *)s + used;
    return v.has_value() ? v.value() : 0.0;
}

/* The front-panel date DISPAK is given at boot.  Only `autotime' reaches it,
 * and booting Unix does not. */
void sim_get_time(SimTime *t)
{
    t->year = 126; /* 2026, as tm_year counted */
    t->mon  = 0;
    t->mday = 1;
    t->hour = 0;
    t->min  = 0;
}

uint32 sim_now_ms(void)
{
    return proc_now();
}

/* ================================================================ the trace */

#define DEB_BUF 8192

static i32 deb_fd = -1;
static char deb_buf[DEB_BUF];
static usize deb_len;

static void deb_put(Sink *, const char *buf, int n)
{
    for (int i = 0; i < n; i++) {
        if (deb_len == DEB_BUF)
            return; /* the driver drains it; a burst past this is dropped */
        deb_buf[deb_len++] = buf[i];
    }
}

static Sink deb_sink = { deb_put };

Sink *deb_file_open(const char *path)
{
    /* The open is a co_await; braam_open_trace() below did it. */
    (void)path;
    return deb_fd >= 0 ? &deb_sink : nullptr;
}

void deb_file_close(void)
{
}

/* ============================================================== the console */

/*
 * The second screen, where the page put one up.  Upstream's tty26 was a telnet
 * line; Sys::TermOpen is where that goes in a browser tab.
 */
static ScreenRef screen2;
static int screen2_ok;

int con_second(void)
{
    return screen2_ok;
}

void con_flush(void)
{
    /* The write is the driver's; con_drain() below does it. */
}

/* The keyboard is claimed in proc_main, where a claim can be awaited, and
 * ~Proc gives it back.  There is no mode to set. */
t_stat con_init(void)
{
    return SCPE_OK;
}

t_stat con_raw(void)
{
    return SCPE_OK;
}

void con_cooked(void)
{
}

/* ============================================================== the driver */

namespace {

/* Where this run's copies of the packs live. */
char g_home[256];

/* A key, as the byte the guest's terminal would have seen.  There are no
 * control characters on Braam: ^C is 'c' with the control modifier. */
i32 key_byte(const Key &k)
{
    if (k.mods & MOD_CTRL) {
        u32 c = k.code;
        if (c >= 'a' && c <= 'z')
            return i32(c - 'a' + 1);
        if (c >= 'A' && c <= 'Z')
            return i32(c - 'A' + 1);
        if (c == '[')
            return 033;
        return -1;
    }
    switch (k.code) {
    case KEY_ENTER:
        return '\r';
    case KEY_BACKSPACE:
        return 0177; /* the guest's erase character */
    case KEY_TAB:
        return '\t';
    case KEY_ESCAPE:
        return 033;
    default:
        break;
    }
    return k.code < 0x80 ? i32(k.code) : -1;
}

/* A task per screen: parked on the next key, feeding the ring the machine
 * drains.  con_get() only looks, which is what lets an instruction call it.
 * One read per screen is the rule -- a key ring has one receiver -- so two
 * screens are two tasks. */
Task<i32> keyboard(int con, ScreenRef on)
{
    for (;;) {
        Result<KeyPress> r = Err(Error::NoMemory);
        if (Task<Result<KeyPress>> t = key_read(on))
            r = co_await t;
        if (r.is_err()) {
            if (r.error() == Error::Again || r.error() == Error::Intr)
                continue;
            co_return 0;
        }
        i32 b = key_byte(Key{ r.value().code, r.value().mods });
        if (b < 0)
            continue;
        if (b == con_stop_char) {
            stop_cpu     = TRUE;
            sim_interval = 0;
            continue;
        }
        con_feed(con, b);
    }
}

/* What con_flush() would have done, where a write is allowed. */
Task<void> con_drain()
{
    const char *buf;
    int n = con_take(CON_SCREEN, &buf);

    if (n > 0)
        if (Task<Result<void>> t = write_all(SYS_STDOUT, Str(buf, usize(n))))
            (void)co_await t;
    /* A write to a screen descriptor is text on that grid, as this is on ours. */
    if (screen2_ok) {
        n = con_take(CON_SCREEN2, &buf);
        if (n > 0)
            if (Task<Result<void>> t = write_all(u32(screen2.at), Str(buf, usize(n))))
                (void)co_await t;
    }
    if (deb_fd >= 0 && deb_len) {
        if (Task<Result<void>> t = write_all(u32(deb_fd), Str(deb_buf, deb_len)))
            (void)co_await t;
        deb_len = 0;
    }
}

/* The runs the machine asked for.  One syscall per run: a zone is 8256 bytes
 * and SYS_READ_MAX is 64 KB, so a page transfer is one read. */
Task<t_stat> io_service_async()
{
    IoRequest *q = &io_request;
    UNIT *u      = q->unit;

    if (!u)
        co_return SCPE_OK;
    q->unit  = NULL;
    Image *m = u->image;

    for (int i = 0; i < q->nrun; i++) {
        IoRun *r    = &q->run[i];
        u64 off     = u64(r->off) * 8;
        usize bytes = usize(r->n) * 8;

        Result<u64> at = Err(Error::NoMemory);
        if (Task<Result<u64>> t = seek_fd(u32(m->fd), i64(off), SYS_SEEK_SET))
            at = co_await t;
        if (at.is_err()) {
            m->err = 1;
            break;
        }

        if (q->write) {
            Result<void> w = Err(Error::NoMemory);
            if (Task<Result<void>> t =
                    write_all(u32(m->fd), Str(reinterpret_cast<const char *>(r->mem), bytes)))
                w = co_await t;
            if (w.is_err()) {
                m->err = 1;
                break;
            }
            continue;
        }

        /* A short read is a zone the image was never written that far into. */
        usize got = 0;
        while (got < bytes) {
            Result<String> chunk = Err(Error::NoMemory);
            if (Task<Result<String>> t = read_some(u32(m->fd), u32(bytes - got)))
                chunk = co_await t;
            if (chunk.is_err() || chunk.value().empty())
                break;
            Str s = chunk.value().str();
            memcpy(reinterpret_cast<char *>(r->mem) + got, s.data(), s.size());
            got += s.size();
        }
        if (got < bytes) {
            if (q->fail)
                *q->fail |= q->fail_mask;
            break;
        }
    }

    if (m->err)
        co_return SCPE_IOERR;
    sim_activate(u, q->delay);
    co_return SCPE_OK;
}

/* ------------------------------------------------------------- the store */

/* Where the package put the images: /pkg/store/simbesm-<version>/share/besm6.
 * BESM6_PREFIX overrides, which is how a test and a hand-built tree find them. */
Task<String> find_data()
{
    String p;

    Str env = proc_env("BESM6_PREFIX");
    if (!env.empty()) {
        p.assign(env);
        co_return p;
    }

    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/besm6"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir  = path_dirname(link.value().str()); /* .../bin */
        Str root = path_dirname(dir);                /* .../simbesm-<version> */
        p.assign(root);
        p.append("/share/besm6");
        co_return p;
    }
    p.assign("/pkg/share/besm6");
    co_return p;
}

/* The packs are written in place, so they cannot be run from the store.  On the
 * first run they are copied to a directory of the program's own; after that the
 * user's Unix is what is there. */
Task<Result<void>> stage(Str from, Str home, bool again)
{
    static const char *const PACKS[] = { "root3072.disk", "usr3100.disk" };

    if (Task<Result<void>> t = make_dir_all(home))
        if ((co_await t).is_err())
            co_return Err(Error::NoMemory);

    for (const char *name : PACKS) {
        Buf<256> dst;
        dst.put(home).put('/').put(Str(name, strlen(name)));

        if (!again) {
            Result<FileInfo> st = Err(Error::NoMemory);
            if (Task<Result<FileInfo>> t = stat_of(dst.str()))
                st = co_await t;
            if (st.is_ok())
                continue; /* already staged */
        }

        Buf<256> src;
        src.put(from).put('/').put(Str(name, strlen(name)));
        if (Task<Result<void>> t = copy_file(src.str(), dst.str()))
            if (Result<void> r = co_await t; r.is_err())
                co_return Err(r.error());
    }
    co_return {};
}

/*
 * The second Consul line's screen.  With no -S the program tries terminal 1 and
 * settles for one console where there is none: a page with one canvas is the
 * ordinary case.
 */
Task<void> open_second(u32 term)
{
    Result<ScreenRef> ref = Err(Error::NoMemory);
    if (Task<Result<ScreenRef>> t = screen_open(term))
        ref = co_await t;
    if (ref.is_err())
        co_return; /* the page put up no such canvas */

    /* Taking is what arbitrates, not opening: a screen whose own shell sits at
     * its prompt holds the keys, and a line needs both halves. */
    Result<Geometry> keys = Err(Error::NoMemory);
    if (Task<Result<Geometry>> t = keys_claim(true, ref.value()))
        keys = co_await t;
    if (keys.is_err())
        co_return;

    screen2    = ref.value();
    screen2_ok = 1;
}

constexpr Str USAGE =
    "Usage:\n"
    "    besm6 [-r] [-S <screen>]\n"
    "Options:\n"
    "    -r          copy the packs from the store again, discarding this Unix\n"
    "    -S <screen> put the second Consul line on this terminal (/proc/terms),\n"
    "                or `none' to turn the line off.  Terminal 1 by default.\n";

constexpr Opts SPEC{ "r", "S" };

} // namespace

Task<i32> proc_main(Args args)
{
    bool again     = false;
    bool no_second = false;
    u32 second     = 1; /* the terminal a dual-screen page puts up beside this one */

    if (help_asked(args))
        co_return co_await usage_asked(USAGE);

    OptParse opts(args, SPEC);
    for (Opt o;;) {
        Result<bool> more = opts.next(o);
        if (more.is_err())
            co_return co_await usage_error(USAGE);
        if (!more.value())
            break;
        if (o.name == 'r') {
            again = true;
            continue;
        }
        if (o.value == "none") {
            no_second = true;
            continue;
        }
        Option<u32> n = parse_u32(o.value);
        if (!n.has_value())
            co_return co_await usage_error(USAGE);
        second = n.value();
    }

    /* Where the images are, and where this run's copies live. */
    String data;
    if (Task<String> t = find_data())
        data = co_await t;

    Str home = proc_env("HOME");
    if (home.empty())
        home = "/home";
    {
        Buf<256> h;
        h.put(home).put("/.besm6");
        usize n = h.str().size();
        if (n >= sizeof(g_home))
            n = sizeof(g_home) - 1;
        memcpy(g_home, h.str().data(), n);
        g_home[n] = 0;
    }
    Str home_dir(g_home, strlen(g_home));

    if (Task<Result<void>> t = stage(data.str(), home_dir, again))
        if (Result<void> r = co_await t; r.is_err()) {
            co_await errln("besm6", "cannot stage the disk images", r.error());
            co_return 1;
        }

    /* The trace file, if BESM6_DEBUG names one.  Opened here because the open
     * is a co_await; sim_debug_from_env() only asks for it. */
    Str deb = proc_env("BESM6_DEBUG");
    if (!deb.empty() && deb != "-") {
        Result<i32> fd = Err(Error::NoMemory);
        if (Task<Result<i32>> t = open_at(deb, SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC))
            fd = co_await t;
        if (fd.is_ok())
            deb_fd = fd.value();
    }

    /* The four images, before the machine is configured: attach_unit() finds
     * them by name, and opening one is a co_await. */
    static const char *const OPEN[] = { "root3072.disk", "usr3100.disk" };
    for (const char *name : OPEN)
        if (Task<Result<void>> t = img_prepare(home_dir, name, false))
            if (Result<void> r = co_await t; r.is_err()) {
                co_await errln("besm6", name, r.error());
                co_return 1;
            }
    static const char *const MAKE[] = { "unix0.drum", "unix1.drum" };
    for (const char *name : MAKE)
        if (Task<Result<void>> t = img_prepare(home_dir, name, true))
            if (Result<void> r = co_await t; r.is_err()) {
                co_await errln("besm6", name, r.error());
                co_return 1;
            }

    /* The kernel image, read whole: sim_load() is a plain function. */
    Buf<256> kpath;
    kpath.put(data.str()).put("/unix");
    String kbytes;
    {
        Result<i32> fd = Err(Error::NoMemory);
        if (Task<Result<i32>> t = open_read(kpath.str()))
            fd = co_await t;
        if (fd.is_err()) {
            co_await errln("besm6", "unix", fd.error());
            co_return 1;
        }
        for (;;) {
            Result<String> c = Err(Error::NoMemory);
            if (Task<Result<String>> t = read_chunk(u32(fd.value())))
                c = co_await t;
            if (c.is_err() || c.value().empty())
                break;
            if (!kbytes.append(c.value().str()))
                break;
        }
        co_await close_fd(u32(fd.value()));
    }

    if (Task<Result<Geometry>> t = keys_claim(true))
        (void)co_await t;
    if (!proc_spawn(keyboard(CON_SCREEN, ScreenRef{}))) {
        co_await errln("besm6", "no room for the keyboard task", Error::NoMemory);
        co_return 1;
    }
    if (!no_second) {
        if (Task<void> t = open_second(second))
            co_await t;
        if (screen2_ok && !proc_spawn(keyboard(CON_SCREEN2, screen2)))
            screen2_ok = 0; /* no task for it: one console, then */
    }

    t_stat r = machine_init();
    if (r != SCPE_OK) {
        co_await con_drain();
        co_return machine_exit(r);
    }
    sink_puts(sim_con, "\nBESM-6 Simulator Demo\n");

    Blob kernel = { reinterpret_cast<const unsigned char *>(kbytes.data()), kbytes.size(), 0 };
    r           = besm6_boot_unix(&kernel);
    if (r != SCPE_OK) {
        sim_printf("%s\n", sim_error_text(r));
        co_await con_drain();
        co_return machine_exit(r);
    }

    sim_run_begin();
    u32 last_yield = proc_now();
    for (;;) {
        r = cpu_burst();

        if (r == REASON_IO) {
            if (Task<t_stat> t = io_service_async())
                r = co_await t;
            if (r != SCPE_OK)
                break;
            continue;
        }
        if (r == REASON_YIELD) {
            co_await con_drain();
            if (stop_cpu) {
                stop_cpu = FALSE;
                r        = SCPE_STOP;
                break;
            }
            /* The park that lets the keyboard task be resumed and gives the
             * native stack back (machine.h).  A burst the clock did not tick
             * over is faster than the clock can measure, so wait a millisecond
             * rather than spin: a process that always asks for zero never lets
             * its worker breathe. */
            u32 t_now  = proc_now();
            u32 wait   = (t_now == last_yield) ? 1 : 0;
            last_yield = t_now;
            if (Task<Result<void>> t = sleep_for(wait))
                (void)co_await t;
            continue;
        }
        break; /* a stop code */
    }
    sim_run_end();

    Buf<128> why;
    why.put('\n').put((r >= SCPE_BASE) ? Str(sim_error_text(r), strlen(sim_error_text(r)))
                                       : Str(sim_stop_messages[r], strlen(sim_stop_messages[r])));
    why.put('\n');
    sink_write(sim_con, why.str().data(), int(why.str().size()));
    co_await con_drain();

    co_return machine_exit(r);
}
