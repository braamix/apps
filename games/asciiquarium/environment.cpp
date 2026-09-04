// environment.go: the water bands, the castle and the seaweed.

#include "quarium.h"

namespace {

// Tiled across the screen, and physical so a bubble pops at the surface.
constexpr Str WATER_SEGMENTS[4] = {
    R"AQ(~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~)AQ",
    R"AQ(^^^^ ^^^  ^^^   ^^^    ^^^^      )AQ",
    R"AQ(^^^^      ^^^^     ^^^    ^^     )AQ",
    R"AQ(^^      ^^^^      ^^^    ^^^^^^  )AQ",
};

constexpr Str CASTLE_SHAPE = R"AQ(               T~~
               |
              /^\
             /   \
 _   _   _  /     \  _   _   _
[ ]_[ ]_[ ]/ _   _ \[ ]_[ ]_[ ]
|_=__-_ =_|_[ ]_[ ]_|_=-___-__|
 | _- =  | =_ = _    |= _=   |
 |= -[]  |- = _ =    |_-=_[] |
 | =_    |= - ___    | =_ =  |
 |=  []- |-  /| |\   |=_ =[] |
 |- =_   | =| | | |  |- = -  |
 |_______|__|_|_|_|__|_______|)AQ";

constexpr Str CASTLE_COLOR = R"AQ(                RR
                W
              Wyyw
             y   y
 W   W   W  yWWWWWy  W   W   W
WW WW WW WW W   W WwWW WW WW WW
WWWWWWW WWWWW W W WWWWWWWWWWWWWW
 W W W  W W W W W    W  W   WWW
 W  W   W  W W W     W W W  WWW
 W  W   W  W WWW     W W W  WWW
 W  W   W  W W W W   W  W   WWW
 W  W   W W W W W W  W  W   WWW
 WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW)AQ";

} // namespace

void add_environment(Animation *anim)
{
    i32 seg_size = i32(WATER_SEGMENTS[0].size());
    i32 repeat   = (anim->Width() / seg_size) + 1;
    for (i32 i = 0; i < 4; i++) {
        String tiled;
        if (!str_repeat(tiled, WATER_SEGMENTS[i], repeat))
            return;
        Str frames[1] = { tiled.str() };
        EntityOptions o;
        o.type          = ET_WATERLINE;
        o.shape         = Span<const Str>(frames, 1);
        o.y             = i + 5;
        o.z             = depth_water_line(i);
        o.default_color = COLOR_CYAN;
        o.physical      = true;
        anim->new_entity(o);
    }
}

// Static scenery, bottom right.
void add_castle(Animation *anim)
{
    Str shape[1] = { CASTLE_SHAPE };
    Str color[1] = { CASTLE_COLOR };
    EntityOptions o;
    o.type  = ET_CASTLE;
    o.shape = Span<const Str>(shape, 1);
    o.color = Span<const Str>(color, 1);
    o.x     = anim->Width() - 32;
    o.y     = anim->Height() - 13;
    o.z     = DEPTH_CASTLE;
    // DARK_GREY.
    o.default_color = COLOR_BLACK | COLOR_BRIGHT;
    anim->new_entity(o);
}

// Two alternating frames; it sways rather than moves. Its death spawns a
// replacement, so the population holds.
void add_seaweed(Entity *, Animation *anim)
{
    String frames[2];
    i32 height = rand_int(4) + 3;
    for (i32 i = 1; i <= height; i++) {
        i32 left  = i % 2;
        i32 right = 1 - left;
        if (!frames[left].append("(\n") || !frames[right].append(" )\n"))
            return;
    }
    i32 max_x = anim->Width() - 2;
    if (max_x < 1)
        max_x = 1;
    i32 x = rand_int(max_x) + 1;
    i32 y = anim->Height() - height;
    if (y < 9)
        y = 9;
    f64 speed = 0.25 + rand_f64() * 0.05;

    // Upstream's DieTime is 8 to 12 minutes of wall clock; the same at
    // upstream's own ten frames a second, and the port then reads no clock.
    i32 life = (8 * 60 + rand_int(4 * 60)) * 10;

    Str shape[2] = { frames[0].str(), frames[1].str() };
    EntityOptions o;
    o.type           = ET_SEAWEED;
    o.shape          = Span<const Str>(shape, 2);
    o.x              = x;
    o.y              = y;
    o.z              = DEPTH_SEAWEED;
    o.args           = true;
    o.frame_step     = speed;
    o.die_frame      = life;
    o.death_callback = add_seaweed;
    o.default_color  = COLOR_GREEN;
    anim->new_entity(o);
}

// A width/15 density rule.
void add_all_seaweed(Animation *anim)
{
    i32 count = anim->Width() / 15;
    for (i32 i = 0; i < count; i++)
        add_seaweed(nullptr, anim);
}
