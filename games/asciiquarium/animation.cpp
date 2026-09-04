// animation.go: the runtime loop, the event handling and the draw pipeline.
//
// The screen is a ProcScreen: a frame is built in a shadow grid and the cells
// that differ are written through, so a paused or unchanged frame sends
// nothing. The keyboard cannot be awaited in the same task as the clock, so it
// is a second one, which is also upstream's goroutine and its channel.

#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "proc/rt.h"
#include "quarium.h"

namespace {

// math/rand's global source. xorshift32; upstream never seeds at all.
u32 rng_state = 1;

// The keys the second task has read, and whether a resize arrived.
constexpr usize RING = 64;

struct KeyRing {
    u32 code[RING];
    usize head;
    usize tail;
    bool resized;
    bool closed;
};

// Namespace scope, so it must be trivially destructible.
KeyRing ring;

void ring_push(u32 code)
{
    usize next = (ring.tail + 1) % RING;
    if (next == ring.head)
        return; // full, as a 64-deep channel drops nothing but cannot block
    ring.code[ring.tail] = code;
    ring.tail            = next;
}

bool ring_pop(u32 &code)
{
    if (ring.head == ring.tail)
        return false;
    code      = ring.code[ring.head];
    ring.head = (ring.head + 1) % RING;
    return true;
}

// Upstream's maskColorMap, which is termbox's sixteen and therefore Braam's.
// 0xff is "not in the map", where the entity's default colour stands.
constexpr u8 NO_COLOR = 0xff;

u8 mask_color(char c)
{
    switch (c) {
    case 'r':
        return COLOR_RED;
    case 'R':
        return COLOR_RED | COLOR_BRIGHT;
    case 'g':
        return COLOR_GREEN;
    case 'G':
        return COLOR_GREEN | COLOR_BRIGHT;
    case 'y':
        return COLOR_YELLOW;
    case 'Y':
        return COLOR_YELLOW | COLOR_BRIGHT;
    case 'b':
        return COLOR_BLUE;
    case 'B':
        return COLOR_BLUE | COLOR_BRIGHT;
    case 'm':
        return COLOR_MAGENTA;
    case 'M':
        return COLOR_MAGENTA | COLOR_BRIGHT;
    case 'c':
        return COLOR_CYAN;
    case 'C':
        return COLOR_CYAN | COLOR_BRIGHT;
    // Upstream's map, the other way round from the pairs above: termbox's
    // ColorLightGray is bright white and its ColorWhite is plain.
    case 'w':
        return COLOR_WHITE | COLOR_BRIGHT;
    case 'W':
        return COLOR_WHITE;
    case 'k':
        return COLOR_BLACK;
    case 'K':
        return COLOR_BLACK | COLOR_BRIGHT;
    // The digits a mask keeps when rand_color did not rewrite it.
    case '1':
        return COLOR_CYAN;
    case '2':
        return COLOR_YELLOW;
    case '3':
        return COLOR_GREEN;
    case '4':
        return COLOR_WHITE;
    case '5':
        return COLOR_RED;
    case '6':
        return COLOR_BLUE;
    case '7':
        return COLOR_MAGENTA;
    case '8':
        return COLOR_BLACK;
    case '9':
        return COLOR_WHITE;
    default:
        return NO_COLOR;
    }
}

constexpr Cell BLANK{ 0, COLOR_WHITE, COLOR_BLACK, 0, 0 };

bool cell_same(const Cell &a, const Cell &b)
{
    return a.ch == b.ch && a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
}

// One rune of the info overlay's frame.
bool is_box_rune(char32_t c)
{
    return c == U'╔' || c == U'═' || c == U'╗' || c == U'║' || c == U'╚' || c == U'╝';
}

// A foreground for one overlay rune: the frame and header accented, the
// controls line green, the closing hint magenta.
u8 info_style_for(usize line_idx, Str line, char32_t ch)
{
    if (ch == U' ')
        return COLOR_WHITE;
    if (line_idx <= 4)
        return is_box_rune(ch) ? COLOR_CYAN : COLOR_WHITE;
    if (line.contains("Q/q quit"))
        return COLOR_GREEN;
    if (line.contains("Press I or ESC"))
        return COLOR_MAGENTA;
    return COLOR_WHITE;
}

usize rune_count(Str s)
{
    usize n = 0;
    for (usize i = 0; i < s.size();) {
        char32_t c;
        i += utf8_decode(s, i, c);
        n++;
    }
    return n;
}

// Parked on the next key for the whole run, because only next_key() reshapes
// the grid and the root task is asleep on the clock.
Task<i32> keyboard(ProcScreen *scr)
{
    for (;;) {
        Result<Key> r = co_await scr->next_key();
        if (r.is_err()) {
            if (r.error() == Error::Intr) {
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
            continue; // ev.Ch is zero for a chord upstream
        ring_push(k.code);
    }
}

} // namespace

void rng_seed(u32 s)
{
    rng_state = s ? s : 1;
}

static u32 rng_next()
{
    u32 x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

// rand.Intn: panics on n <= 0 upstream, and every call site passes a positive.
i32 rand_int(i32 n)
{
    if (n <= 1)
        return 0;
    return i32(rng_next() % u32(n));
}

// rand.Float64: [0, 1), 53 bits' worth from two draws.
f64 rand_f64()
{
    u64 hi = u64(rng_next()) >> 5; // 27 bits
    u64 lo = u64(rng_next()) >> 6; // 26 bits
    return f64((hi << 26) | lo) * (1.0 / 9007199254740992.0);
}

bool str_repeat(String &out, Str s, i32 n)
{
    out.clear();
    for (i32 i = 0; i < n; i++)
        if (!out.append(s))
            return false;
    return true;
}

i32 depth_water_line(i32 i)
{
    switch (i) {
    case 0:
        return DEPTH_WATER_LINE0;
    case 1:
        return DEPTH_WATER_LINE1;
    case 2:
        return DEPTH_WATER_LINE2;
    case 3:
        return DEPTH_WATER_LINE3;
    default:
        return DEPTH_WATER_LINE0;
    }
}

// ------------------------------------------------------------ the entities

Entity *Animation::new_entity(const EntityOptions &opts)
{
    Entity *e = heap_new<Entity>();
    if (!e)
        return nullptr;

    e->type           = opts.type;
    e->x              = f64(opts.x);
    e->y              = f64(opts.y);
    e->z              = f64(opts.z);
    e->callback       = opts.callback;
    e->die_frame      = opts.die_frame;
    e->die_offscreen  = opts.die_offscreen;
    e->death_callback = opts.death_callback;
    e->default_color  = opts.default_color;
    e->physical       = opts.physical;
    e->coll_handler   = opts.coll_handler;
    e->auto_trans     = opts.auto_trans;
    e->mode           = opts.mode;

    // A nil CallbackArgs is {0, 0, 0, 0.5}.
    if (opts.args) {
        e->dx         = opts.dx;
        e->dy         = opts.dy;
        e->dz         = opts.dz;
        e->frame_step = opts.frame_step;
    } else if (opts.mode == HOOK_NONE) {
        e->frame_step = 0.5;
    }

    for (const Str &s : opts.shape) {
        Frame f;
        if (!f.assign(s) || !e->shapes.push(static_cast<Frame &&>(f))) {
            heap_delete(e);
            return nullptr;
        }
    }
    for (const Str &s : opts.color) {
        Frame f;
        if (!f.assign(s) || !e->colors.push(static_cast<Frame &&>(f))) {
            heap_delete(e);
            return nullptr;
        }
    }
    // NewEntity's normalisation of an empty slice.
    if (e->shapes.empty()) {
        Frame f;
        if (!f.assign(Str()) || !e->shapes.push(static_cast<Frame &&>(f))) {
            heap_delete(e);
            return nullptr;
        }
    }
    if (e->colors.empty()) {
        Frame f;
        if (!f.assign(Str()) || !e->colors.push(static_cast<Frame &&>(f))) {
            heap_delete(e);
            return nullptr;
        }
    }
    e->size_from_first_frame();

    add_entity(e);
    return e;
}

// Kept ascending by z. An insertion sort, since the list is short and adds
// are one at a time; sort.Slice on every add was the same thing more loudly.
void Animation::add_entity(Entity *e)
{
    if (!e)
        return;
    usize at = entities.size();
    while (at > 0 && entities[at - 1]->z > e->z)
        at--;
    if (!entities.insert(at, e))
        heap_delete(e);
}

// Unlinked at once, freed at the end of the frame. Go kept a removed entity
// alive for whoever still held it, and two still do: the snapshot animate()
// walks, and the collision lists built before the death sweep.
void Animation::del_entity(Entity *e)
{
    for (usize i = 0; i < entities.size(); i++) {
        if (entities[i] != e)
            continue;
        entities.erase(i);
        for (Entity *o : entities)
            for (usize k = o->collision.size(); k-- > 0;)
                if (o->collision[k] == e)
                    o->collision.erase(k);
        if (!dead.push(e))
            heap_delete(e);
        return;
    }
}

void Animation::bury_dead()
{
    for (Entity *e : dead)
        heap_delete(e);
    dead.clear();
}

void Animation::remove_all_entities()
{
    for (Entity *e : entities)
        heap_delete(e);
    entities.clear();
    bury_dead();
}

void Animation::entities_by_type(EntityType t, Vec<Entity *> &out) const
{
    out.clear();
    for (Entity *e : entities)
        if (e->type == t)
            out.push(e);
}

// Overlaps by rectangle, O(n^2) and readable. Only a physical entity keeps a
// list, but it is tested against every other.
void Animation::check_collisions()
{
    for (Entity *e : entities)
        e->collision.clear();
    for (Entity *e : entities) {
        if (!e->physical)
            continue;
        i32 ex = e->at_x(), ey = e->at_y(), ew = e->width, eh = e->height;
        for (Entity *o : entities) {
            if (e == o)
                continue;
            i32 ox = o->at_x(), oy = o->at_y(), ow = o->width, oh = o->height;
            if (ex < ox + ow && ex + ew > ox && ey < oy + oh && ey + eh > oy)
                e->collision.push(o);
        }
    }
}

// ---------------------------------------------------------------- the frame

// Shape and mask are read line by line in parallel; a mask letter picks a
// colour and a transparent cell is skipped.
void Animation::draw_entity(const Entity *e)
{
    i32 x              = e->at_x();
    i32 y              = e->at_y();
    const Frame &shape = e->current_shape();
    const Frame &color = e->current_color();

    for (usize li = 0; li < shape.lines.size(); li++) {
        i32 dy = y + i32(li);
        if (dy < 0 || dy >= height)
            continue;
        Str line = shape.lines[li].str();
        Str mask = li < color.lines.size() ? color.lines[li].str() : Str();
        for (usize ci = 0; ci < line.size(); ci++) {
            char ch = line[ci];
            i32 dx  = x + i32(ci);
            if (dx < 0 || dx >= width)
                continue;
            if (e->auto_trans && (ch == ' ' || ch == e->transparent))
                continue;
            if (u8(ch) < 32)
                continue;
            u8 fg = e->default_color;
            if (ci < mask.size()) {
                u8 m = mask_color(mask[ci]);
                if (m != NO_COLOR)
                    fg = m;
            }
            Cell &c = shadow[usize(dy) * usize(width) + usize(dx)];
            c.ch    = char32_t(u8(ch));
            c.fg    = fg;
            c.bg    = COLOR_BLACK;
            c.attrs = 0;
        }
    }
}

// The cells that differ, and the damage that follows from them.
void Animation::blit()
{
    Grid &g = screen->grid();
    if (shadow.size() != usize(g.cols) * usize(g.rows))
        return;
    for (u32 y = 0; y < g.rows; y++)
        for (u32 x = 0; x < g.cols; x++) {
            Cell &src = shadow[usize(y) * usize(g.cols) + usize(x)];
            Cell *dst = g.at(x, y);
            if (!whole && cell_same(*dst, src))
                continue;
            *dst = src;
            if (!whole)
                g.touch(x, y, 1, 1);
        }
    if (whole) {
        g.touch(0, 0, g.cols, g.rows);
        whole = false;
    }
}

// Painted back to front: the list is ascending by z, and lower z is in front.
void Animation::draw_frame()
{
    for (Cell &c : shadow)
        c = BLANK;
    for (usize i = entities.size(); i-- > 0;)
        draw_entity(entities[i]);
    blit();
}

// The help text, centred over the aquarium.
void Animation::draw_info_overlay()
{
    for (Cell &c : shadow)
        c = BLANK;

    Span<const Str> lines = info_lines();
    i32 start_y           = (height - i32(lines.size())) / 2;
    if (start_y < 0)
        start_y = 0;
    for (usize i = 0; i < lines.size(); i++) {
        i32 y = start_y + i32(i);
        if (y >= height)
            break;
        Str ln = lines[i];
        i32 x  = (width - i32(rune_count(ln))) / 2;
        if (x < 0)
            x = 0;
        i32 ci = 0;
        for (usize at = 0; at < ln.size(); ci++) {
            char32_t ch;
            at += utf8_decode(ln, at, ch);
            if (x + ci >= width)
                break;
            Cell &c = shadow[usize(y) * usize(width) + usize(x + ci)];
            c.ch    = ch;
            c.fg    = info_style_for(i, ln, ch);
            c.bg    = COLOR_BLACK;
            c.attrs = 0;
        }
    }
    blit();
}

// --------------------------------------------------------------- the world

// Size-dependent scenery is rebuilt; everything else is kept and clamped.
void Animation::reflow_for_resize()
{
    for (usize i = entities.size(); i-- > 0;) {
        Entity *e = entities[i];
        if (e->type == ET_WATERLINE || e->type == ET_CASTLE || e->type == ET_SEAWEED) {
            entities.erase(i);
            heap_delete(e);
            continue;
        }
        i32 max_y = height - e->height;
        if (max_y < 0)
            max_y = 0;
        if (e->y < 0)
            e->y = 0;
        if (e->y > f64(max_y))
            e->y = f64(max_y);
    }
    add_environment(this);
    add_castle(this);
    add_all_seaweed(this);
}

// One simulation step, then the frame. The list is walked over a snapshot of
// the pointers, so a callback may add or remove.
void Animation::animate()
{
    Vec<Entity *> snap;
    for (Entity *e : entities)
        snap.push(e);
    for (Entity *e : snap)
        e->update(this);

    check_collisions();

    snap.clear();
    for (Entity *e : entities)
        snap.push(e);
    for (Entity *e : snap) {
        if (!e->should_die(width, height))
            continue;
        if (e->death_callback)
            e->death_callback(e, this);
        del_entity(e);
    }
    bury_dead();
    draw_frame();
}

// The whole screen. Upstream keeps a row back for a terminal's bottom-row
// scroll; a cell written on the last row of a grid moves nothing.
bool Animation::size_to(u32 cols, u32 rows)
{
    width      = i32(cols);
    height     = i32(rows);
    usize want = usize(cols) * usize(rows);
    shadow.clear();
    if (!shadow.reserve(want))
        return false;
    for (usize i = 0; i < want; i++)
        shadow.push(BLANK);
    whole = true;
    return true;
}

void setup_aquarium(Animation *anim)
{
    add_environment(anim);
    add_castle(anim);
    add_all_seaweed(anim);
    add_all_fish(anim);
    random_object(nullptr, anim);
}

// ---------------------------------------------------------------- the loop

namespace {

// Says so on the real stderr, so the claims go back first: a message written
// while the alternate screen is held dies with it.
Task<i32> too_small(i32 w, i32 h)
{
    if (Task<Result<Geometry>> t = screen_claim(false))
        co_await t;
    if (Task<Result<Geometry>> t = keys_claim(false))
        co_await t;
    Buf<96> b;
    b.put("asciiquarium: terminal too small: need at least 40x15, got ");
    b.put(int(w));
    b.put('x');
    b.put(int(h));
    b.put('\n');
    co_await write_all(SYS_STDERR, b.str());
    co_return 1;
}

} // namespace

Task<i32> quarium_run(Animation *anim)
{
    ProcScreen *scr = heap_new<ProcScreen>();
    if (!scr) {
        co_await errln("asciiquarium", "the screen", Error::NoMemory);
        co_return 1;
    }
    if ((co_await scr->take_keys()).is_err()) {
        co_await write_all(SYS_STDERR, "asciiquarium: no keyboard\n");
        co_return 1;
    }
    if ((co_await scr->take_screen()).is_err()) {
        co_await write_all(SYS_STDERR, "asciiquarium: no screen\n");
        co_return 1;
    }
    anim->screen = scr;

    Grid &g = scr->grid();
    if (g.cols < 40 || g.rows < 15)
        co_return co_await too_small(i32(g.cols), i32(g.rows));
    if (!anim->size_to(g.cols, g.rows)) {
        co_await errln("asciiquarium", "the frame", Error::NoMemory);
        co_return 1;
    }

    setup_aquarium(anim);

    if (!proc_spawn(keyboard(scr))) {
        co_await errln("asciiquarium", "the keyboard task", Error::NoMemory);
        co_return 1;
    }

    bool paused       = false;
    bool showing_info = false;

    while (anim->running) {
        // termbox.EventError: the keyboard is gone, and so is the aquarium.
        if (ring.closed) {
            co_await errln("asciiquarium", "the keyboard", Error::Closed);
            co_return 1;
        }
        if (ring.resized) {
            ring.resized = false;
            if (g.cols < 40 || g.rows < 15)
                co_return co_await too_small(i32(g.cols), i32(g.rows));
            if (!anim->size_to(g.cols, g.rows))
                co_return 1;
            anim->reflow_for_resize();
            if (showing_info)
                anim->draw_info_overlay();
            else
                anim->draw_frame();
        }

        for (u32 code; ring_pop(code);) {
            if (code == KEY_ESCAPE) {
                if (showing_info) {
                    showing_info = false;
                    paused       = false;
                }
                continue;
            }
            switch (code) {
            case 'q':
            case 'Q':
                anim->running = false;
                break;
            case 'r':
            case 'R':
                anim->remove_all_entities();
                setup_aquarium(anim);
                break;
            case 'p':
            case 'P':
                if (!showing_info)
                    paused = !paused;
                break;
            case 'i':
            case 'I':
                showing_info = !showing_info;
                if (showing_info) {
                    paused = true;
                    anim->draw_info_overlay();
                } else
                    paused = false;
                break;
            default:
                break;
            }
        }
        if (!anim->running)
            break;

        if (showing_info)
            anim->draw_info_overlay();
        else if (!paused)
            anim->animate();

        if ((co_await scr->flush()).is_err())
            anim->whole = true; // the frame was refused; ask for it whole

        Result<void> slept = co_await sleep_for(100);
        if (slept.is_err() && slept.error() != Error::Intr)
            co_return 130; // ^C, which upstream had no handler for either
    }
    co_return 0;
}
