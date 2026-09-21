/*
 * ASCII clock — Dey, 2019
 * https://github.com/Deybacsi/asciiclock
 *
 * Linux console ANSI output is a ProcScreen here; wall time is clock_now().
 */

#include "clock.h"
#include "clock_digits.h"
#include "kernel/alloc.h"
#include "kernel/key.h"
#include "kernel/screen.h"
#include "kernel/str.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/screen.h"
#include "proc/time.h"
#include "proc/usage.h"

namespace {

constexpr Str USAGE =
    "Usage:\n"
    "    asciiclock\n"
    "    asciiclock -h|--help\n"
    "Keys:\n"
    "    q, x, ESC    quit\n"
    "    f            toggle the 3D cube and pick a new background\n"
    "    ^C           quit\n";

u32 rand_state = 1;

void put2(char *out, u32 v)
{
    out[0] = char('0' + (v / 10) % 10);
    out[1] = char('0' + v % 10);
    out[2] = 0;
}

} // namespace

void clock_srand(u32 seed)
{
    rand_state = seed ? seed : 1;
}

int clock_rand()
{
    rand_state = rand_state * 1103515245u + 12345u;
    return int((rand_state / 65536u) % 32768u);
}

static void seed_from_env()
{
    Str env = proc_env("ASCIICLOCK_SEED");
    if (!env.empty()) {
        u32 seed = 0;
        for (usize i = 0; i < env.size(); i++) {
            if (env[i] < '0' || env[i] > '9')
                break;
            seed = seed * 10 + u32(env[i] - '0');
        }
        clock_srand(seed);
        return;
    }
    clock_srand(proc_random());
}

int ACT_HOUR[MAXTIMEZONES];
int ACT_MIN[MAXTIMEZONES];
int ACT_SEC[MAXTIMEZONES];
char ACT_TIMESTR[MAXTIMEZONES][7];
char ACT_MINSTR[MAXTIMEZONES][5];
char LAST_TIMESTR[MAXTIMEZONES][7];
char LAST_MINSTR[MAXTIMEZONES][5];

int ACT_BG_EFFECT = 3;
int ACT_FG_EFFECT = 0;

const int BG_EFFECTNO = 9;
const int FG_EFFECTNO = 1;

EffectFn background[][3] = {
    { init_bg_snow, calc_bg_snow, draw_bg_snow },
    { init_bg_star, calc_bg_star, draw_bg_star },
    { init_bg_star3d, calc_bg_star3d, draw_bg_star3d },
    { init_bg_plasma, calc_bg_plasma, draw_bg_plasma },
    { init_bg_matrix, calc_bg_matrix, draw_bg_matrix },
    { init_bg_fire, calc_bg_fire, draw_bg_fire },
    { init_bg_labyrinth, calc_bg_labyrinth, draw_bg_labyrinth },
    { init_bg_gof, calc_bg_gof, draw_bg_gof },
    { init_bg_obj3d, calc_bg_obj3d, draw_bg_obj3d },
};

EffectFn foreground[][3] = {
    { init_fg_cube, calc_fg_cube, draw_fg_cube },
};

static Task<Result<void>> checktime()
{
    Result<Clock> got = Err(Error::NoMemory);
    if (Task<Result<Clock>> t = clock_now())
        got = co_await t;
    if (got.is_err())
        co_return Err(got.error());

    i32 tz   = got.value().tz_min;
    i64 secs = i64(got.value().epoch_ms / 1000) + i32(tz) * 60;
    Civil c  = civil(secs);

    ACT_HOUR[0] = int(c.hour);
    ACT_MIN[0]  = int(c.min);
    ACT_SEC[0]  = int(c.sec);
    put2(ACT_TIMESTR[0], c.hour);
    put2(ACT_TIMESTR[0] + 2, c.min);
    put2(ACT_TIMESTR[0] + 4, c.sec);
    put2(ACT_MINSTR[0], c.hour);
    put2(ACT_MINSTR[0] + 2, c.min);
    co_return {};
}

void init_all()
{
    clearalllayer(CLEARCHAR);

    ACT_BG_EFFECT = clock_rand() % BG_EFFECTNO;
    background[ACT_BG_EFFECT][0]();
    foreground[ACT_FG_EFFECT][0]();
}

namespace {

constexpr usize RING = 64;

struct KeyRing {
    int code[RING];
    usize head;
    usize tail;
    bool resized;
    bool closed;
    bool quit;
};

KeyRing ring;

void ring_push(int code)
{
    usize next = (ring.tail + 1) % RING;
    if (next == ring.head)
        return;
    ring.code[ring.tail] = code;
    ring.tail            = next;
}

bool ring_pop(int &code)
{
    if (ring.head == ring.tail)
        return false;
    code      = ring.code[ring.head];
    ring.head = (ring.head + 1) % RING;
    return true;
}

Task<i32> keyboard(ProcScreen *s)
{
    for (;;) {
        Result<Key> r = co_await s->next_key();
        if (r.is_err()) {
            if (r.error() == Error::Intr) {
                if (sig_take(SIG_INT))
                    ring.quit = true;
                else
                    ring.resized = true;
                continue;
            }
            if (r.error() == Error::Again)
                continue;
            ring.closed = true;
            co_return 0;
        }
        const Key &k = r.value();
        if (k.mods & (MOD_CTRL | MOD_ALT))
            continue;
        ring_push(int(k.code));
    }
}

static u32 frame_ms(u32 elapsed)
{
    if (elapsed >= u32(FPS2MILLISEC))
        return 1;
    u32 wait = u32(FPS2MILLISEC) - elapsed;
    return wait ? wait : 1;
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (help_asked(args))
        co_return co_await usage_asked(USAGE);
    if (args.size() > 1)
        co_return co_await usage_error(USAGE);

    seed_from_env();

    ProcScreen *scr = heap_new<ProcScreen>();
    if (!scr) {
        co_await errln("asciiclock", "the screen", Error::NoMemory);
        co_return 1;
    }
    if ((co_await scr->take_keys()).is_err()) {
        co_await write_all(SYS_STDERR, "asciiclock: no keyboard\n");
        co_return 1;
    }
    if ((co_await scr->take_screen()).is_err()) {
        co_await write_all(SYS_STDERR, "asciiclock: no screen\n");
        co_return 1;
    }

    Grid &g = scr->grid();
    initscreen(int(g.cols), int(g.rows));
    g.cursor_on = false;

    init_clock_digital();
    init_all();

    if (Task<Result<void>> ct = checktime())
        co_await ct;
    for (int i = 0; i < 6; i++)
        LAST_TIMESTR[0][i] = ACT_TIMESTR[0][i];
    LAST_MINSTR[0][0] = ACT_MINSTR[0][0];
    LAST_MINSTR[0][1] = ACT_MINSTR[0][1];
    LAST_MINSTR[0][2] = ACT_MINSTR[0][2];
    LAST_MINSTR[0][3] = ACT_MINSTR[0][3];

    bool show_foreground = false;
    bool running         = true;

    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    if (!proc_spawn(keyboard(scr))) {
        co_await errln("asciiclock", "the keyboard task", Error::NoMemory);
        co_return 1;
    }

    while (running) {
        if (ring.closed) {
            co_await errln("asciiclock", "the keyboard", Error::Closed);
            co_return 1;
        }
        if (ring.quit)
            break;

        if (ring.resized) {
            ring.resized = false;
            initscreen(int(g.cols), int(g.rows));
            init_clock_digital();
            init_all();
        }

        u32 t0 = proc_now();

        if (Task<Result<void>> ct = checktime()) {
            Result<void> r = co_await ct;
            if (r.is_err()) {
                if (r.error() == Error::Cancelled)
                    co_return 130;
                co_return 1;
            }
        }

        if (LAST_MINSTR[0][0] != ACT_MINSTR[0][0] || LAST_MINSTR[0][1] != ACT_MINSTR[0][1] ||
            LAST_MINSTR[0][2] != ACT_MINSTR[0][2] || LAST_MINSTR[0][3] != ACT_MINSTR[0][3]) {
            init_clock_digital();
            init_all();
        }

        background[ACT_BG_EFFECT][1]();
        background[ACT_BG_EFFECT][2]();

        int cx = (SCREENX - int(DIGITDESIGNS[ACTDIGITDESIGN].x * 5)) / 2;
        int cy = (SCREENY - int(DIGITDESIGNS[ACTDIGITDESIGN].y)) / 2;
        draw_clock_digital(cx, cy);

        if (show_foreground) {
            foreground[ACT_FG_EFFECT][1]();
            foreground[ACT_FG_EFFECT][2]();
        }

        mergelayers();
        paint_final_to_grid(g);
        g.cursor_on = false;

        Result<void> flushed = co_await scr->flush();
        if (flushed.is_err()) {
            if (flushed.error() == Error::Cancelled)
                co_return 130;
            co_return 1;
        }

        int keypress;
        while (ring_pop(keypress)) {
            if (keypress == 27 || keypress == 'q' || keypress == 'x') {
                running = false;
                break;
            }
            if (keypress == 'f') {
                show_foreground = !show_foreground;
                init_all();
            }
        }

        LAST_MINSTR[0][0] = ACT_MINSTR[0][0];
        LAST_MINSTR[0][1] = ACT_MINSTR[0][1];
        LAST_MINSTR[0][2] = ACT_MINSTR[0][2];
        LAST_MINSTR[0][3] = ACT_MINSTR[0][3];

        u32 elapsed        = proc_now() - t0;
        Result<void> slept = co_await sleep_for(frame_ms(elapsed));
        if (slept.is_err()) {
            if (slept.error() == Error::Intr && sig_take(SIG_INT))
                break;
            if (slept.error() == Error::Cancelled)
                co_return 130;
            if (slept.error() != Error::Intr)
                co_return 1;
        }
        if (ring.quit)
            break;
    }

    co_return 0;
}
