#include "fs/path.h"
#include "kernel/alloc.h"
#include "kernel/text.h"
#include "kernel/vec.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/usage.h"

// IOCCC 2012/endoh1, Yusuke Endoh's "ASCII fluid dynamics": smoothed-particle
// hydrodynamics over a text file, drawn with marching squares. Ported from
// endoh1.alt2.c, upstream's indent(1)ed rendering of endoh1.c; -c is the
// density colouring of endoh1_color.c.

namespace {

constexpr Str USAGE =
    "Usage:\n"
    "    asciifluid [-cG] [-g <n>] [-p <n>] [-v <n>] [-d <ms>] [<file>]\n"
    "    asciifluid -l\n"
    "Options:\n"
    "    -c    colour the fluid by density\n"
    "    -G    gravity along the imaginary axis, which is sideways\n"
    "    -g    factor of gravity (1)\n"
    "    -p    factor of pressure (4)\n"
    "    -v    factor of viscosity (8)\n"
    "    -d    milliseconds between frames (12)\n"
    "    -l    list the configurations the package ships\n"
    "A '#' is a wall particle, any other non-space a free one, and a name with\n"
    "no '/' that is not a file is looked for among the bundled ones.\n";

// -------------------------------------------------------------- 2D vectors

// Upstream is <complex.h>, which is not here: a complex multiply or divide is
// __muldc3 / __divdc3, and those are compiler-rt.
struct Cx {
    f64 re = 0, im = 0;
};

inline Cx operator+(Cx a, Cx b)
{
    return { a.re + b.re, a.im + b.im };
}

inline Cx operator-(Cx a, Cx b)
{
    return { a.re - b.re, a.im - b.im };
}

inline Cx operator*(Cx a, Cx b)
{
    return { a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re };
}

// The conjugate form, which is what clang inlines a complex division to.
inline Cx operator/(Cx a, Cx b)
{
    f64 q = b.re * b.re + b.im * b.im;
    return { (a.re * b.re + a.im * b.im) / q, (a.im * b.re - a.re * b.im) / q };
}

inline Cx operator+(Cx a, f64 b)
{
    return { a.re + b, a.im };
}

inline Cx operator-(f64 a, Cx b)
{
    return { a - b.re, -b.im };
}

inline void operator+=(Cx &a, Cx b)
{
    a = a + b;
}

inline f64 cabs(Cx a)
{
    return __builtin_sqrt(a.re * a.re + a.im * a.im);
}

// Upstream takes the integer part of the real part by assigning a complex to
// an int. The truncation is load-bearing: 0 < (int)(1 - w) is |d| <= 2.
inline i32 re_int(Cx a)
{
    return i32(a.re);
}

// -------------------------------------------------------------- the state

// Five slots a particle, upstream's layout.
enum : usize { POS = 0, WALL = 1, DENS = 2, FORCE = 3, VEL = 4, SLOTS = 5 };

// Heap, not Vec: a namespace-scope global must be trivially destructible.

// The axes are swapped: real is the row, imaginary is minus the column.
Cx *a    = nullptr;
usize na = 0;

// Upstream's b: the escape prefix, then a byte a cell holding a marching
// square's bits and then the glyph they choose.
char *b = nullptr;

// -c: what each cell's neighbours contributed to its density, and the escapes
// the frame is written as.
i8 *shade   = nullptr;
char *paint = nullptr;

template <class T>
T *renew(T *p, usize n)
{
    if (p)
        heap_free(p);
    T *q = static_cast<T *>(heap_alloc(n * sizeof(T)));
    if (q)
        __builtin_memset(q, 0, n * sizeof(T));
    return q;
}

// The palette an accumulator byte maps to, upstream's ctanh compression.
u8 hue[256];

// Room for "\x1b[2J\x1b[1;1H"; a frame after the first starts at 4.
constexpr usize PRE = 10;

// A cell's escape and glyph: "\x1b[48;5;NNNm" and one byte.
constexpr usize CELL = 12;

// How long the opening frame stands.
constexpr u32 PAUSE = 1000;

u32 cols = 80, rows = 24;
Cx gravity{ 1, 0 };
f64 pressure = 4, viscosity = 8;
u32 delay   = 12;
bool colour = false;

constexpr char GLYPH[] = " '`-.|//,\\|\\_\\/#";

// ------------------------------------------------------------- the reader

// Two particles a non-space character, the second half a row below the first.
bool read_conf(Str s)
{
    usize n = 0;
    for (usize i = 0; i < s.size() && s[i]; i++)
        if (u8(s[i]) > 32)
            n++;
    na = n * 2 * SLOTS;
    if (na && !(a = renew(a, na)))
        return false;

    Cx w{ 0, 0 };
    usize at = 0;
    for (usize i = 0; i < s.size(); i++) {
        i32 x = u8(s[i]);
        if (x == 0)
            break;
        if (x <= 10) {
            w = Cx{ f64(re_int(w + 2)), 0 }; // the next row, column zero
            continue;
        }
        if (x > 32) {
            Cx wall{ f64(x == '#'), 0 };
            a[at + POS]          = w;
            a[at + WALL]         = wall;
            a[at + SLOTS + POS]  = w + 1;
            a[at + SLOTS + WALL] = wall;
            at += 2 * SLOTS;
        }
        w = w - Cx{ 0, 1 };
    }
    return true;
}

// ------------------------------------------------------------ the physics

// Not a coroutine, and not inlined into one: the hot loop's locals belong on
// the stack.
__attribute__((noinline)) void step()
{
    usize n = na;

    for (usize i = 0; i < n; i += SLOTS) {
        a[i + DENS] = a[i + WALL] * Cx{ 9, 0 };
        for (usize j = 0; j < n; j += SLOTS) {
            Cx d = a[i + POS] - a[j + POS];
            Cx w{ cabs(d) / 2 - 1, 0 };
            if (re_int(1 - w) > 0)
                a[i + DENS] += w * w;
        }
    }

    for (usize i = 0; i < n; i += SLOTS) {
        a[i + FORCE] = gravity;
        for (usize j = 0; j < n; j += SLOTS) {
            Cx d = a[i + POS] - a[j + POS];
            Cx w{ cabs(d) / 2 - 1, 0 };
            if (re_int(1 - w) > 0)
                a[i + FORCE] +=
                    w *
                    (d * (3 - a[i + DENS] - a[j + DENS]) * Cx{ pressure, 0 } +
                     a[i + VEL] * Cx{ viscosity, 0 } - a[j + VEL] * Cx{ viscosity, 0 }) /
                    a[i + DENS];
        }
    }

    __builtin_memset(b + PRE, 0, cols * rows);
    if (colour)
        __builtin_memset(shade, 0, cols * rows);

    for (usize i = 0; i < n; i += SLOTS) {
        i32 x = re_int(a[i + POS] * Cx{ 0, 1 });
        i32 y = re_int(a[i + POS] / Cx{ 2, 0 });
        a[i + VEL] += a[i + FORCE] / Cx{ 10, 0 } * Cx{ f64(a[i + WALL].re == 0), 0 };
        a[i + POS] += a[i + VEL];
        if (x < 0 || x >= i32(cols) - 1 || y < 0 || y >= i32(rows) - 1)
            continue;
        // The corners of one 2x2 square, and upstream assigns the last.
        usize c     = usize(x) + usize(y) * cols;
        char *t     = b + PRE + c;
        t[0]        = char(t[0] | 8);
        t[1]        = char(t[1] | 4);
        t[cols]     = char(t[cols] | 2);
        t[cols + 1] = 1;
        if (colour) {
            i8 v                = i8(re_int(a[i + DENS]));
            shade[c]            = i8(shade[c] + v);
            shade[c + 1]        = i8(shade[c + 1] + v);
            shade[c + cols]     = i8(shade[c + cols] + v);
            shade[c + cols + 1] = i8(shade[c + cols + 1] + v);
        }
    }

    for (u32 i = 0; i < cols * rows; i++)
        b[PRE + i] = i % cols == cols - 1 ? '\n' : GLYPH[u8(b[PRE + i])];
}

// -------------------------------------------------------------- the frame

// The escapes -c writes, one run a cell.
void repaint()
{
    char *o = paint + PRE;
    for (u32 i = 0; i < cols * rows; i++, o += CELL) {
        u32 n = hue[u8(shade[i])];
        o[0]  = '\x1b';
        o[1]  = '[';
        o[2]  = '4';
        o[3]  = '8';
        o[4]  = ';';
        o[5]  = '5';
        o[6]  = ';';
        o[7]  = char('0' + n / 100);
        o[8]  = char('0' + n / 10 % 10);
        o[9]  = char('0' + n % 10);
        o[10] = 'm';
        o[11] = b[PRE + i];
    }
}

// Upstream paints twenty-five rows; the last row's newline is dropped instead,
// so the frame is exactly the screen and nothing scrolls.
Str frame(bool first)
{
    usize wide = colour ? CELL : 1;
    usize off  = first ? 0 : 4;
    return Str((colour ? paint : b) + off, PRE + cols * rows * wide - wide - off);
}

bool size_to(u32 c, u32 r)
{
    cols = c < 2 ? 2 : c;
    rows = r < 2 ? 2 : r;
    if (!(b = renew(b, PRE + cols * rows)))
        return false;
    __builtin_memcpy(b, "\x1b[2J\x1b[1;1H", PRE);
    if (!colour)
        return true;
    if (!(shade = renew(shade, cols * rows)))
        return false;
    if (!(paint = renew(paint, PRE + cols * rows * CELL)))
        return false;
    __builtin_memcpy(paint, "\x1b[2J\x1b[1;1H", PRE);
    return true;
}

// Upstream's 6x6x6 cube index for a cell's density: a component every twenty
// down the accumulator, which wraps as a char.
void make_hue()
{
    for (u32 i = 0; i < 256; i++) {
        i8 t  = i8(i);
        i32 y = 16;
        for (u32 k = 0; k < 3; k++) {
            t          = i8(t - 20);
            f64 v      = f64(t) < 0 ? -f64(t) : f64(t);
            i32 weight = k == 0 ? 1 : k == 1 ? 6 : 36;
            y += i32(tanh(9 - v / 2) * 3 + 3) * weight;
        }
        hue[i] = u8(y);
    }
}

// ---------------------------------------------------------------- the data

// Where the package put the configurations. ASCIIFLUID_PREFIX overrides.
Task<String> find_data()
{
    String p;

    Str env = proc_env("ASCIIFLUID_PREFIX");
    if (!env.empty()) {
        p.assign(env);
        co_return p;
    }

    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/asciifluid"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir  = path_dirname(link.value().str()); // .../bin
        Str root = path_dirname(dir);                // .../asciifluid-<version>
        p.assign(root);
        p.append("/share");
        co_return p;
    }

    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir("/pkg/store"))
        ents = co_await t;
    if (ents.is_ok())
        for (const DirEntry &e : ents.value())
            if (e.name.str().starts_with("asciifluid-")) {
                p.assign("/pkg/store/");
                p.append(e.name.str());
                p.append("/share");
                co_return p;
            }

    p.assign("/pkg/share/asciifluid");
    co_return p;
}

Task<bool> openable(Str path)
{
    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_read(path))
        fd = co_await t;
    if (fd.is_err())
        co_return false;
    co_await close_fd(u32(fd.value()));
    co_return true;
}

// A path as given, else the bundled name it may be.
Task<Result<String>> resolve(Str name)
{
    String p;
    if (!p.assign(name))
        co_return Err(Error::NoMemory);
    if (name.find('/') != Str::npos || co_await openable(name))
        co_return p;

    String dir = co_await find_data();
    for (u32 k = 0; k < 2; k++) {
        p.clear();
        if (!p.assign(dir.str()) || !p.push('/') || !p.append(name))
            co_return Err(Error::NoMemory);
        if (k == 0 && !p.append(".txt"))
            co_return Err(Error::NoMemory);
        if (co_await openable(p.str()))
            co_return p;
    }
    if (!p.assign(name))
        co_return Err(Error::NoMemory);
    co_return p;
}

Task<i32> list_conf()
{
    String dir = co_await find_data();

    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir(dir.str()))
        ents = co_await t;
    if (ents.is_err()) {
        co_await errln("asciifluid", dir.str(), ents.error());
        co_return 1;
    }

    String out;
    for (const DirEntry &e : ents.value()) {
        Str n = e.name.str();
        if (!n.ends_with(".txt"))
            continue;
        if (!out.append(n.substr(0, n.size() - 4)) || !out.push('\n'))
            co_return 1;
    }
    if ((co_await write_all(SYS_STDOUT, out.str())).is_err())
        co_return 1;
    co_return 0;
}

// The configuration, read whole. An empty `path` is stdin.
Task<Result<void>> load(Str path)
{
    Str one[1] = { path };
    Args named{ Span<const Str>(one, path.empty() ? 0 : 1) };
    Input in(named, SYS_STDIN, "asciifluid");
    String text;
    for (;;) {
        Result<String> chunk = Err(Error::NoMemory);
        if (Task<Result<String>> t = in.read())
            chunk = co_await t;
        if (chunk.is_err()) {
            if (chunk.error() == Error::Closed)
                break;
            co_return Err(chunk.error());
        }
        if (!text.append(chunk.value().str()))
            co_return Err(Error::NoMemory);
    }
    if (!read_conf(text.str()))
        co_return Err(Error::NoMemory);
    co_return {};
}

// ---------------------------------------------------------------- the loop

Task<Result<Geometry>> screen_of()
{
    Result<TtyInfo> tty = Err(Error::Unsupported);
    if (Task<Result<TtyInfo>> t = tty_of(SYS_STDOUT))
        tty = co_await t;
    if (tty.is_err() || !tty.value().console)
        co_return Err(Error::Unsupported);

    // The claim is what puts the shell's screen back: ^C kills us, and a
    // killed process runs no destructor of its own.
    Result<Geometry> got = Err(Error::NoMemory);
    if (Task<Result<Geometry>> t = screen_claim(true))
        got = co_await t;
    if (got.is_err())
        co_return Err(got.error());
    co_return got.value().cols ? got.value() : tty.value().at;
}

Task<i32> play()
{
    Result<Geometry> at = co_await screen_of();
    if (at.is_ok() ? !size_to(at.value().cols, at.value().rows) : !size_to(cols, rows)) {
        co_await errln("asciifluid", "the frame", Error::NoMemory);
        co_return 1;
    }
    if (Task<Result<void>> t = sig_catch(SIG_WINCH))
        co_await t;

    // The first frame clears; the rest home and repaint, upstream's o = b + 4.
    bool first   = true;
    bool opening = true;
    for (;;) {
        step();
        if (colour)
            repaint();
        if ((co_await write_all(SYS_STDOUT, frame(first))).is_err())
            co_return 1;
        first = false;

        // A second on the opening frame. Not upstream's, and not what -d sets.
        u32 ms  = opening ? PAUSE : delay;
        opening = false;

        Result<void> slept = Err(Error::NoMemory);
        if (Task<Result<void>> t = sleep_for(ms))
            slept = co_await t;
        if (slept.is_ok())
            continue;
        // A resize abandons the sleep; the next frame is the new shape.
        if (slept.error() == Error::Intr && sig_take(SIG_WINCH)) {
            Result<Geometry> now = Err(Error::Unsupported);
            if (Task<Result<TtyInfo>> t = tty_of(SYS_STDOUT)) {
                Result<TtyInfo> tty = co_await t;
                if (tty.is_ok() && tty.value().console)
                    now = tty.value().at;
            }
            if (now.is_ok() && !size_to(now.value().cols, now.value().rows))
                co_return 1;
            first = true;
            continue;
        }
        co_return 130; // ^C, the only way out of an endless run
    }
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (help_asked(args))
        co_return co_await usage_asked(USAGE);

    bool list = false, sideways = false;
    f64 g = 1;
    OptParse parse(args, Opts{ "cGl", "gpvd" });
    for (Opt o;;) {
        Result<bool> r = parse.next(o);
        if (r.is_err())
            co_return co_await usage_error(USAGE);
        if (!r.value())
            break;
        if (o.name == 'c')
            colour = true;
        else if (o.name == 'G')
            sideways = true;
        else if (o.name == 'l')
            list = true;
        else if (o.name == 'd') {
            Option<u32> n = parse_u32(o.value);
            if (!n.has_value())
                co_return co_await usage_error(USAGE);
            delay = n.value();
        } else {
            Option<f64> n = parse_f64(o.value);
            if (!n.has_value())
                co_return co_await usage_error(USAGE);
            if (o.name == 'g')
                g = n.value();
            else if (o.name == 'p')
                pressure = n.value();
            else
                viscosity = n.value();
        }
    }
    gravity = sideways ? Cx{ 0, g } : Cx{ g, 0 };

    if (list)
        co_return co_await list_conf();

    Args rest = parse.rest();
    String path;
    if (rest.size() > 0) {
        Result<String> found = co_await resolve(rest[0]);
        if (found.is_err())
            co_return 1;
        path = static_cast<String &&>(found.value());
    }

    Result<void> got = Err(Error::NoMemory);
    if (Task<Result<void>> t = load(path.str()))
        got = co_await t;
    if (got.is_err())
        co_return got.error() == Error::Cancelled ? 130 : 1;

    if (na == 0) {
        co_await errln("asciifluid", "no particles in the configuration", Error::Invalid);
        co_return 1;
    }
    if (colour)
        make_hue();

    co_return co_await play();
}
