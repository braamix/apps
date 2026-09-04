// The entity model, the animation runtime, and the small pieces of Go the
// port had to write out: the split frames, the string helpers and the dice.
#pragma once

#include "kernel/screen.h"
#include "kernel/span.h"
#include "kernel/str.h"
#include "kernel/string.h"
#include "kernel/types.h"
#include "kernel/vec.h"
#include "proc/io.h"
#include "proc/screen.h"

struct Animation;
struct Entity;

// A custom behaviour, a hit, and what runs just before removal. Upstream's
// three func types, and here they are plain pointers: there are no closures,
// so what a Go literal captured is a field or an argument instead.
using EntityCallback         = bool (*)(Entity *, Animation *);
using EntityCollisionHandler = void (*)(Entity *, Animation *);
using EntityDeathHandler     = void (*)(Entity *, Animation *);

// Depth is a small layer legend used while drawing. Lower depth is drawn
// last, so it is in front; depth.go's comment says the opposite and the code
// is what to trust.
enum : i32 {
    DEPTH_GUI_TEXT   = 0,
    DEPTH_GUI        = 1,
    DEPTH_SHARK      = 2,
    DEPTH_FISH_START = 3,
    DEPTH_FISH_END   = 20,
    DEPTH_SEAWEED    = 21,
    DEPTH_CASTLE     = 22,

    DEPTH_WATER_LINE3 = 2,
    DEPTH_WATER_GAP3  = 3,
    DEPTH_WATER_LINE2 = 4,
    DEPTH_WATER_GAP2  = 5,
    DEPTH_WATER_LINE1 = 6,
    DEPTH_WATER_GAP1  = 7,
    DEPTH_WATER_LINE0 = 8,
    DEPTH_WATER_GAP0  = 9,
};

// Depth["water_line<i>"], with upstream's fallback for a name that is not one.
i32 depth_water_line(i32 i);

// Upstream compares EntityType strings; the set is closed, so it is an enum.
// An entity upstream left unnamed is ET_NONE.
enum EntityType : u8 {
    ET_NONE = 0,
    ET_WATERLINE,
    ET_CASTLE,
    ET_SEAWEED,
    ET_FISH,
    ET_BUBBLE,
    ET_SHARK,
    ET_TEETH,
    ET_FISHLINE,
    ET_FISHHOOK,
    ET_HOOK_POINT,
    ET_DOLPHIN,
};

// The fishhook's map[string]string{"mode": ...}. HOOK_NONE means the entity
// carries movement arguments instead, which is the other arm of upstream's
// type switch on CallbackArgs.
enum HookMode : u8 {
    HOOK_NONE = 0,
    HOOK_LOWERING,
    HOOK_HOOKED,
};

// One animation frame, split into lines once at construction rather than by
// strings.Split on every entity on every frame.
struct Frame {
    Vec<String> lines;

    bool assign(Str text);
};

// What NewEntity is given. Upstream's NewEntityOptions, and a designated
// initialiser reads the way its struct literal did.
struct EntityOptions {
    EntityType type = ET_NONE;

    Span<const Str> shape;
    Span<const Str> color;

    i32 x = 0, y = 0, z = 0;

    EntityCallback callback = nullptr;

    // CallbackArgs. `args` is upstream's []float64 being present at all: nil
    // becomes {0, 0, 0, 0.5}, and a three-element literal leaves step at zero.
    bool args      = false;
    f64 dx         = 0;
    f64 dy         = 0;
    f64 dz         = 0;
    f64 frame_step = 0;
    HookMode mode  = HOOK_NONE;

    i32 die_frame                     = 0;
    bool die_offscreen                = false;
    EntityDeathHandler death_callback = nullptr;

    u8 default_color                    = COLOR_WHITE;
    bool physical                       = false;
    EntityCollisionHandler coll_handler = nullptr;
    bool auto_trans                     = false;
};

// Everything needed for one on-screen object: sprite frames, movement data,
// collision data and life rules. Fish, bubbles, hooks and decorations all use
// this one type.
struct Entity {
    EntityType type = ET_NONE;

    Vec<Frame> shapes;
    Vec<Frame> colors;

    f64 x = 0, y = 0, z = 0;

    EntityCallback callback = nullptr;

    f64 dx = 0, dy = 0, dz = 0, frame_step = 0;
    HookMode mode = HOOK_NONE;

    bool die_offscreen                = false;
    i32 die_frame                     = 0;
    EntityDeathHandler death_callback = nullptr;

    u8 default_color                    = COLOR_WHITE;
    bool physical                       = false;
    EntityCollisionHandler coll_handler = nullptr;
    bool auto_trans                     = false;
    char transparent                    = ' ';

    i32 current_frame = 0;
    f64 frame_time    = 0;
    i32 frame_count   = 0;
    Vec<Entity *> collision;
    bool alive = true;
    i32 width = 0, height = 0;

    // The integer coordinates drawing and collisions use. Go truncates toward
    // zero on the cast, and so does this.
    i32 at_x() const { return i32(x); }

    i32 at_y() const { return i32(y); }

    const Frame &current_shape() const;

    const Frame &current_color() const;

    // The default movement, for an entity with no callback of its own.
    bool move_entity();

    void kill() { alive = false; }

    bool should_die(i32 screen_width, i32 screen_height) const;

    void update(Animation *anim);

    void size_from_first_frame();
};

// The main runtime: the screen, the active entities, and the loop flags.
struct Animation {
    Vec<Entity *> entities;
    Vec<Entity *> dead; // unlinked this frame, freed at the end of it
    bool classic = false;
    bool running = true;
    i32 width = 0, height = 0;

    ProcScreen *screen = nullptr;
    Vec<Cell> shadow; // the frame being built, diffed into the grid
    bool whole = true;

    ~Animation() { remove_all_entities(); }

    i32 Width() const { return width; }

    i32 Height() const { return height; }

    // Builds an entity and adds it to the world. Null when the heap said no,
    // which upstream had no way to be told.
    Entity *new_entity(const EntityOptions &opts);

    void add_entity(Entity *e);

    void del_entity(Entity *e);

    void remove_all_entities();

    // Frees what del_entity unlinked. Once a frame, when nothing holds them.
    void bury_dead();

    // The objects carrying one type label, as pointers into the live list.
    void entities_by_type(EntityType t, Vec<Entity *> &out) const;

    void check_collisions();

    void draw_entity(const Entity *e);

    // The cells that differ, and the damage that follows.
    void blit();

    void draw_frame();

    void draw_info_overlay();

    void reflow_for_resize();

    void animate();

    bool size_to(u32 cols, u32 rows);
};

void setup_aquarium(Animation *anim);

// Claims the screen and the keyboard, spawns the key task, and runs the clock.
Task<i32> quarium_run(Animation *anim);

// environment.cpp
void add_environment(Animation *anim);
void add_castle(Animation *anim);
void add_seaweed(Entity *dead, Animation *anim);
void add_all_seaweed(Animation *anim);

// fish.cpp
void add_bubble(Entity *fish, Animation *anim);
void bubble_collision(Entity *bubble, Animation *anim);
bool fish_callback(Entity *fish, Animation *anim);
void fish_collision(Entity *fish, Animation *anim);
void add_splat(Animation *anim, i32 x, i32 y, i32 z);
void add_fish(Entity *dead, Animation *anim);
void add_all_fish(Animation *anim);

// A mask's digits 1..9 become random colour letters, one draw a digit, so a
// fish's colours vary without its art changing.
bool rand_color(String &out, Str mask);

// special.cpp
void add_shark(Entity *dead, Animation *anim);
void add_ship(Entity *dead, Animation *anim);
void add_whale(Entity *dead, Animation *anim);
void add_monster(Entity *dead, Animation *anim);
void add_big_fish(Entity *dead, Animation *anim);
void add_fishhook(Entity *dead, Animation *anim);
void add_swan(Entity *dead, Animation *anim);
void add_ducks(Entity *dead, Animation *anim);
void add_dolphins(Entity *dead, Animation *anim);
void random_object(Entity *dead, Animation *anim);
void retract(Entity *e, Animation *anim);

// info.cpp
Str info_text();
Span<const Str> info_lines();
Str version_string();

// The dice. math/rand's global source, which upstream never seeds;
// ASCIIQUARIUM_SEED pins it, the way adventure's ADVENTURE_SEED does.
void rng_seed(u32 s);
i32 rand_int(i32 n);
f64 rand_f64();

// strings.Repeat, and what the whale does to indent a spout.
bool str_repeat(String &out, Str s, i32 n);
