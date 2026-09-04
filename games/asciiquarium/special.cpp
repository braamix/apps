// special.go: the shark, the ship, the whale, the monsters, the big fish, the
// fishhook, the swan, the ducks and the dolphins — and the router that chains
// one into the next.

#include "quarium.h"

namespace {

constexpr Str SHARK_SHAPE[2] = {
    R"AQ(                              __
                             ( `\
  ,                          )   `\
;' `.                       (     `\__
 ;   `.             __..---''          `~~~~-._
  `.   `.____...--''                       (b  `--._
    >                     _.-'      .((      ._     )
  .`.-`--...__         .-'     -.___.....-(|/|/|/|/'
 ;.'         `. ...----`.___.',,,_______......---'
 '           '-')AQ",
    R"AQ(                     __
                    /' )
                  /'   (                          ,
              __/'     )                       .' `;
      _.-~~~~'          ``---..__             .'   ;
 _.--'  b)                       ``--...____.'   .'
(     _.      )).      `-._                     <
 `\|\|\|\|)-.....___.-     `-.         __...--'-.'.
   `---......_______,,,`.___.'----... .'         `.;
                                     `-`           `)AQ",
};

constexpr Str SHARK_COLOR[2] = {
    R"AQ(




                                           cR
 
                                          cWWWWWWWW


)AQ",
    R"AQ(



        Rc

  WWWWWWWWc



)AQ",
};

constexpr Str SHIP_SHAPE[2] = {
    R"AQ(     |    |    |
    )_)  )_)  )_)
   )___))___))___)\
  )____)____)_____)\\
_____|____|____|____\\\__
\                   /)AQ",
    R"AQ(         |    |    |
        (_(  (_(  (_(
      /(___((___((___(
    //(_____(____(____(
__///____|____|____|_____
    \                   /)AQ",
};

constexpr Str SHIP_COLOR[2] = {
    R"AQ(     y    y    y

                  w
                   ww
yyyyyyyyyyyyyyyyyyyywwwyy
y                   y)AQ",
    R"AQ(         y    y    y

      w
    ww
yywwwyyyyyyyyyyyyyyyyyyyy
    y                   y)AQ",
};

constexpr Str WHALE_SHAPE[2] = {
    R"AQ(        .-----:
      .'       `.
,    /       (o) \
\`._/          ,__))AQ",
    R"AQ(    :-----.
  .'       `.
 / (o)       \    ,
(__,          \_.'/)AQ",
};

constexpr Str WHALE_COLOR[2] = {
    R"AQ(             C C
           CCCCCCC
           C  C  C
        BBBBBBB
      BB       BB
B    B       BWB B
BBBBB          BBBB)AQ",
    R"AQ(   C C
 CCCCCCC
 C  C  C
    BBBBBBB
  BB       BB
 B BWB       B    B
BBBB          BBBBB)AQ",
};

constexpr Str WATER_SPOUTS[7] = {
    R"AQ(


   :)AQ",
    R"AQ(

   :
   :)AQ",
    R"AQ(
  . .
  -:-
   :)AQ",
    R"AQ(
  . .
 .-:-.
   :)AQ",
    R"AQ(
  . .
'.-:-.`
'  :  ')AQ",
    R"AQ(

 .- -.
;  :  ;)AQ",
    R"AQ(


;     ;)AQ",
};

constexpr Str NEW_MONSTER_SHAPE[2][2] = {
    { R"AQ(
         _   _                   _   _       _a_a
       _{.`=`.}_     _   _     _{.`=`.}_    {/ ''\_
 _    {.'  _  '.}   {.`'`.}   {.'  _  '.}  {|  ._oo)
{ \  {/  .'~'.  \}  {/ .-. \}  {/  .'~'.  \} {/  |)AQ",
      R"AQ(
                      _   _                    _a_a
  _      _   _     _{.`=`.}_     _   _      {/ ''\_
 { \    {.`'`.}   {.'  _  '.}   {.`'`.}    {|  ._oo)
  \ \  {/ .-. \}  {/  .'~'.  \}  {/ .-. \}   {/  |)AQ" },
    { R"AQ(
   a_a_       _   _                   _   _
 _/'' \}    _{.`=`.}_     _   _     _{.`=`.}_
(oo_.  |}  {.'  _  '.}   {.`'`.}   {.'  _  '.}    _
    |  \} {/  .'~'.  \}  {/ .-. \}  {/  .'~'.  \}  / })AQ",
      R"AQ(
   a_a_                    _   _
 _/'' \}      _   _     _{.`=`.}_     _   _      _
(oo_.  |}    {.`'`.}   {.'  _  '.}   {.`'`.}    / }
    |  \}   {/ .-. \}  {/  .'~'.  \}  {/ .-. \}  / /)AQ" },
};

constexpr Str NEW_MONSTER_COLOR[2] = {
    R"AQ(
                                                W W



)AQ",
    R"AQ(
   W W



)AQ",
};

constexpr Str OLD_MONSTER_SHAPE[2][4] = {
    { R"AQ(
                                                          ____
            __                                          /   o  \
          /    \        _                     _       /     ____ >
  _      |  __  |     /   \        _        /   \   |     |
 | \     |  ||  |    |     |     /   \    |     |  |     |)AQ",
      R"AQ(
                                                          ____
                                             __         /   o  \
             _                     _       /    \     /     ____ >
   _       /   \        _        /   \   |  __  |   |     |
  | \     |     |     /   \    |     |  |  ||  |   |     |)AQ",
      R"AQ(
                                                          ____
                                  __                  /   o  \
 _                      _       /    \        _     /     ____ >
| \          _        /   \   |  __  |     /   \  |     |
 \ \       /   \    |     |  |  ||  |    |     | |     |)AQ",
      R"AQ(
                                                          ____
                       __                             /   o  \
  _          _       /    \        _                /     ____ >
 | \       /   \   |  __  |     /   \        _    |     |
  \ \     |     |  |  ||  |    |     |     /   \  |     |)AQ" },
    { R"AQ(
    ____
  /  o   \                                          __
< ____     \       _                     _        /    \
      |     |   /   \        _        /   \     |  __  |      _
      |     |  |     |     /   \    |     |    |  ||  |     / |)AQ",
      R"AQ(
    ____
  /  o   \         __
< ____     \     /    \       _                     _
      |     |   |  __  |    /   \        _        /   \       _
      |     |   |  ||  |   |     |     /   \     |     |     / |)AQ",
      R"AQ(
    ____
  /  o   \                  __
< ____     \     _        /    \       _                      _
      |     |  /   \     |  __  |   /   \        _          / |
      |     | |     |    |  ||  |  |     |    /   \       / /)AQ",
      R"AQ(
    ____
  /  o   \                             __
< ____     \                _        /    \       _          _
      |     |    _        /   \     |  __  |   /   \       / |
      |     |  /   \    |     |    |  ||  |  |     |     / /)AQ" },
};

constexpr Str OLD_MONSTER_COLOR[2] = {
    R"AQ(

                                                            W


)AQ",
    R"AQ(

     W


)AQ",
};

constexpr Str BIG_FISH1_SHAPE[2] = {
    R"AQ( ______
`"".  `````-----.....__
     `.  .      .       `-.
       :     .     .       `.
 ,     :   .    .          _ :
: `.   :                  (@) `._
 `. `..'     .     =`-.       .__)
   ;     .        =  ~  :     .-"
 .' .'`.   .    .  =.-'  `._ .'
: .'   :               .   .'
 '   .'  .    .     .   .-'
   .'____....----''.'=.'
   ""             .'.'
               ''"'`)AQ",
    R"AQ(                           ______
          __.....-----'''''  .-""'
       .-'       .      .  .'
     .'       .     .     :
    : _          .    .   :     ,
 _.' (@)                  :   .' :
(__.       .-'=     .     `..' .'
 "-.     :  ~  =        .     ;
   `. _.'  `-.=  .    .   .'`. `.
     `.   .               :   `. :
       `-.   .     .    .  `.   `
          `.=`.``----....____`.
            `.`.             ""
              '`"``)AQ",
};

constexpr Str BIG_FISH1_COLOR[2] = {
    R"AQ( 111111
11111  11111111111111111
     11  2      2       111
       1     2     2       11
 1     1   2    2          1 1
1 11   1                  1W1 111
 11 1111     2     1111       1111
   1     2        1  1  1     111
 11 1111   2    2  1111  111 11
1 11   1               2   11
 1   11  2    2     2   111
   111111111111111111111
   11             1111
               11111)AQ",
    R"AQ(                           111111
          11111111111111111  11111
       111       2      2  11
     11       2     2     1
    1 1          2    2   1     1
 111 1W1                  1   11 1
1111       1111     2     1111 11
 111     1  1  1        2     1
   11 111  1111  2    2   1111 11
     11   2               1   11 1
       111   2     2    2  11   1
          111111111111111111111
            1111             11
              11111)AQ",
};

constexpr Str BIG_FISH2_SHAPE[2] = {
    R"AQ(                _ _ _
             .='\ \ \`"=,
           .'\ \ \ \ \ \ \
\'=._     / \ \ \_\_\_\_\_\
\'=._'.  /\ \,-"`- _ - _ - '-.
  \`=._\|'.\/- _ - _ - _ - _- \
  ;"= ._\=./_ -_ -_ {`"=_    @ \
   ;="_-_=- _ -  _ - {"=_"-     \
   ;_=_--_.,          {_.='   .-/
  ;.="` / ';\        _.     _.-`
  /_.='/ \/ /;._ _ _{.-;`/"
/._=_.'   '/ / / / /{.= /
/.='       `'./_/_.=`{_/)AQ",
    R"AQ(            _ _ _
        ,="`/ / /'=. 
       / / / / / / /'.
      /_/_/_/_/_/ / / \     _.='/
   .-' - _ - _ -`"-,/ /\  .'_.='/
  / -_ - _ - _ - _ -\/.'|/_.=`/
 / @    _="`} _- _- _\.=/_. =";
/     -"_="}  - _  - _ -=_-_"=;
\-.   '=._}          ,._--_=_; 
 `-._     ._        /;' \ `"=.;
     `"\`;-.}_ _ _.;\ \/ \'=._\
        \ =.}\ \ \ \ \'   '._=_.\
         \_}`=._\_\.'`       '=.\)AQ",
};

constexpr Str BIG_FISH2_COLOR[2] = {
    R"AQ(                1 1 1
             1111 1 11111
           111 1 1 1 1 1 1
11111     1 1 1 11111111111
1111111  11 111112 2 2 2 2 111
  111111111112 2 2 2 2 2 2 22 1
  111 1111 12 22 22 11111    W 1
   11111112 2 2  2 2 111111     1
   111111111          11111   111
  11111 11111        11     1111
  111111 11 1111 1 111111111
1111111   11 1 1 1 1111 1
1111       1111111111111)AQ",
    R"AQ(            1 1 1
        11111 1 1111
       1 1 1 1 1 1 111
      11111111111 1 1 1     11111
   111 2 2 2 2 211111 11  1111111
  1 22 2 2 2 2 2 2 211111111111
 1 W    11111 22 22 2111111 111
1     111111 2 2  2 2 21111111
111   11111          111111111
 1111     11        111 1 11111
     111111111 1 1111 11 111111
        1 1111 1 1 1 11   1111111
         1111111111111       1111)AQ",
};

constexpr Str HOOK_SHAPE = R"AQ(       o
      ||
      ||
/ \   ||
  \__//
  `--')AQ";

constexpr Str HOOK_POINT_SHAPE = R"AQ(.
 
\
 )AQ";

constexpr Str DUCK_SHAPE[2][3] = {
    { R"AQ(      _          _          _
,____(')=  ,____(')=  ,____(')<
 \~~= ')    \~~= ')    \~~= '))AQ",
      R"AQ(      _          _          _
,____(')=  ,____(')<  ,____(')=
 \~~= ')    \~~= ')    \~~= '))AQ",
      R"AQ(      _          _          _
,____(')<  ,____(')=  ,____(')=
 \~~= ')    \~~= ')    \~~= '))AQ" },
    { R"AQ(  _          _          _
>(')____,  =(')____,  =(')____,
 (` =~~/    (` =~~/    (` =~~/)AQ",
      R"AQ(  _          _          _
=(')____,  >(')____,  =(')____,
 (` =~~/    (` =~~/    (` =~~/)AQ",
      R"AQ(  _          _          _
=(')____,  =(')____,  >(')____,
 (` =~~/    (` =~~/    (` =~~/)AQ" },
};

constexpr Str DUCK_COLOR[2] = {
    R"AQ(      g          g          g
wwwwwgcgy  wwwwwgcgy  wwwwwgcgy
 wwww Ww    wwww Ww    wwww Ww)AQ",
    R"AQ(  g          g          g
ygcgwwwww  ygcgwwwww  ygcgwwwww
 wW wwww    wW wwww    wW wwww)AQ",
};

constexpr Str DOLPHIN_SHAPE[2][2] = {
    { R"AQ(        ,
      __)\
(\_.-'    a`-.
(/~~````(/~^^`)AQ",
      R"AQ(        ,
(\__  __)\
(/~.''    a`-.
    ````\)~^^`)AQ" },
    { R"AQ(     ,
   _/(__
.-'a    `-._/)
'^^~\)''''~~\))AQ",
      R"AQ(     ,
   _/(__  __/)
.-'a    ``.~\)
'^^~(/'''')AQ" },
};

constexpr Str DOLPHIN_COLOR[2] = {
    R"AQ(


          W)AQ",
    R"AQ(


   W)AQ",
};

constexpr Str SWAN_SHAPE[2] = {
    R"AQ(       ___
,_    / _,\
| \   \( \|
|  \_  \\
(_   \_) \
(\_   `   \
 \   -=~  /)AQ",
    R"AQ( ___
/,_ \    _,
|/ )/   / |
  //  _/  |
 / ( /   _)
/   `   _/)
\  ~=-   /)AQ",
};

constexpr Str SWAN_COLOR[2] = {
    R"AQ(

         g
         yy



)AQ",
    R"AQ(

 g
yy



)AQ",
};

// The fishhook rig: line, hook, and the point a fish catches on.
enum : i32 {
    HOOK_LINE_HEIGHT = 50,
    HOOK_TOP_CLAMP_Y = -10,
    HOOK_POINT_DY    = 2,
    HOOK_LINE_DY     = -HOOK_LINE_HEIGHT,
};

i32 max_int(i32 a, i32 b)
{
    return a > b ? a : b;
}

// Removes the teeth when the shark leaves, then starts the next event.
void shark_death(Entity *, Animation *anim)
{
    Vec<Entity *> teeth;
    anim->entities_by_type(ET_TEETH, teeth);
    for (Entity *t : teeth)
        anim->del_entity(t);
    random_object(nullptr, anim);
}

void add_new_monster(Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 2.0;
    i32 x     = -54;
    if (dir == 1) {
        speed = -2.0;
        x     = anim->Width() - 2;
    }
    Str color[2] = { NEW_MONSTER_COLOR[dir], NEW_MONSTER_COLOR[dir] };
    EntityOptions o;
    o.shape          = Span<const Str>(NEW_MONSTER_SHAPE[dir], 2);
    o.color          = Span<const Str>(color, 2);
    o.x              = x;
    o.y              = 2;
    o.z              = DEPTH_WATER_GAP2;
    o.args           = true;
    o.dx             = speed;
    o.frame_step     = 0.25;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.default_color  = COLOR_GREEN;
    o.auto_trans     = true;
    anim->new_entity(o);
}

void add_old_monster(Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 2.0;
    i32 x     = -64;
    if (dir == 1) {
        speed = -2.0;
        x     = anim->Width() - 2;
    }
    Str color[4] = { OLD_MONSTER_COLOR[dir], OLD_MONSTER_COLOR[dir], OLD_MONSTER_COLOR[dir],
                     OLD_MONSTER_COLOR[dir] };
    EntityOptions o;
    o.shape          = Span<const Str>(OLD_MONSTER_SHAPE[dir], 4);
    o.color          = Span<const Str>(color, 4);
    o.x              = x;
    o.y              = 2;
    o.z              = DEPTH_WATER_GAP2;
    o.args           = true;
    o.dx             = speed;
    o.frame_step     = 0.25;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.default_color  = COLOR_GREEN;
    o.auto_trans     = true;
    anim->new_entity(o);
}

void add_big_fish1(Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 3.0;
    i32 x     = -34;
    if (dir == 1) {
        speed = -3.0;
        x     = anim->Width() - 1;
    }
    i32 max_height = 9;
    i32 min_height = anim->Height() - 15;
    i32 y          = max_height;
    if (min_height > max_height)
        y = max_height + rand_int(min_height - max_height + 1);

    String mask;
    if (!rand_color(mask, BIG_FISH1_COLOR[dir]))
        return;
    Str shape[1] = { BIG_FISH1_SHAPE[dir] };
    Str color[1] = { mask.str() };
    EntityOptions o;
    o.shape          = Span<const Str>(shape, 1);
    o.color          = Span<const Str>(color, 1);
    o.x              = x;
    o.y              = y;
    o.z              = DEPTH_SHARK;
    o.args           = true;
    o.dx             = speed;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.default_color  = COLOR_YELLOW;
    o.auto_trans     = true;
    anim->new_entity(o);
}

void add_big_fish2(Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 2.5;
    i32 x     = -33;
    if (dir == 1) {
        speed = -2.5;
        x     = anim->Width() - 1;
    }
    i32 max_height = 9;
    i32 min_height = anim->Height() - 14;
    i32 y          = max_height;
    if (min_height > max_height)
        y = max_height + rand_int(min_height - max_height + 1);

    String mask;
    if (!rand_color(mask, BIG_FISH2_COLOR[dir]))
        return;
    Str shape[1] = { BIG_FISH2_SHAPE[dir] };
    Str color[1] = { mask.str() };
    EntityOptions o;
    o.shape          = Span<const Str>(shape, 1);
    o.color          = Span<const Str>(color, 1);
    o.x              = x;
    o.y              = y;
    o.z              = DEPTH_SHARK;
    o.args           = true;
    o.dx             = speed;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.default_color  = COLOR_YELLOW;
    o.auto_trans     = true;
    anim->new_entity(o);
}

bool fishhook_callback(Entity *e, Animation *anim);

// The hook's next Y, which the rig parts read to stay anchored to it.
f64 next_hook_y(Animation *anim, f64 y, HookMode hook_mode)
{
    if (hook_mode == HOOK_HOOKED) {
        y -= 2;
        if (y < f64(HOOK_TOP_CLAMP_Y))
            y = f64(HOOK_TOP_CLAMP_Y);
        return y;
    }
    f64 max_depth = f64(i32(f64(anim->Height()) * 0.75));
    y += 2;
    if (y > max_depth)
        y = max_depth;
    return y;
}

// "lowering" goes down to the maximum depth; "hooked" reels up to the clamp.
bool fishhook_callback(Entity *e, Animation *anim)
{
    if (e->type == ET_FISHLINE || e->type == ET_HOOK_POINT) {
        Vec<Entity *> hooks;
        anim->entities_by_type(ET_FISHHOOK, hooks);
        if (!hooks.empty()) {
            Entity *hook = hooks[0];
            f64 target   = next_hook_y(anim, hook->y, hook->mode);
            e->y =
                e->type == ET_FISHLINE ? target + f64(HOOK_LINE_DY) : target + f64(HOOK_POINT_DY);
            return true;
        }
    }
    e->y = next_hook_y(anim, e->y, e->mode);
    return true;
}

// The hook's death takes the rest of the rig with it.
void fishhook_death(Entity *, Animation *anim)
{
    Vec<Entity *> found;
    anim->entities_by_type(ET_HOOK_POINT, found);
    for (Entity *o : found)
        anim->del_entity(o);
    anim->entities_by_type(ET_FISHLINE, found);
    for (Entity *o : found)
        anim->del_entity(o);
    random_object(nullptr, anim);
}

// Default movement, but DieOffscreen is armed only once the sprite has been
// on the screen: a formation member starting fully outside must not be
// removed before it enters.
bool dolphin_delay_offscreen_death(Entity *e, Animation *anim)
{
    bool moved = e->move_entity();
    if (e->die_offscreen)
        return moved;
    f64 sw = f64(anim->Width());
    f64 sh = f64(anim->Height());
    if (e->x + f64(e->width) >= 0 && e->x < sw && e->y + f64(e->height) >= 0 && e->y < sh)
        e->die_offscreen = true;
    return moved;
}

} // namespace

// Two linked entities: the art, and the teeth that do the biting.
void add_shark(Entity *, Animation *anim)
{
    i32 direction = rand_int(2);
    i32 x         = -53;
    i32 y         = 9;
    i32 teeth_x   = -9;
    i32 teeth_y   = y + 7;
    f64 speed     = 2.0;
    if (anim->Height() > 19) {
        y       = 9 + rand_int(max_int(1, anim->Height() - 19) + 1);
        teeth_y = y + 7;
    }
    if (direction == 1) {
        speed   = -2.0;
        x       = anim->Width() - 2;
        teeth_x = x + 9;
    }
    Str teeth[1] = { "*" };
    EntityOptions teeth_opts;
    teeth_opts.type     = ET_TEETH;
    teeth_opts.shape    = Span<const Str>(teeth, 1);
    teeth_opts.x        = teeth_x;
    teeth_opts.y        = teeth_y;
    teeth_opts.z        = DEPTH_SHARK + 1;
    teeth_opts.args     = true;
    teeth_opts.dx       = speed;
    teeth_opts.physical = true;
    anim->new_entity(teeth_opts);
    Str shape[1] = { SHARK_SHAPE[direction] };
    Str color[1] = { SHARK_COLOR[direction] };
    EntityOptions shark_opts;
    shark_opts.type           = ET_SHARK;
    shark_opts.shape          = Span<const Str>(shape, 1);
    shark_opts.color          = Span<const Str>(color, 1);
    shark_opts.x              = x;
    shark_opts.y              = y;
    shark_opts.z              = DEPTH_SHARK;
    shark_opts.args           = true;
    shark_opts.dx             = speed;
    shark_opts.die_offscreen  = true;
    shark_opts.death_callback = shark_death;
    shark_opts.default_color  = COLOR_CYAN;
    shark_opts.auto_trans     = true;
    anim->new_entity(shark_opts);
}

void add_ship(Entity *, Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 1.0;
    i32 x     = -24;
    if (dir == 1) {
        speed = -1;
        x     = anim->Width() - 2;
    }
    Str shape[1] = { SHIP_SHAPE[dir] };
    Str color[1] = { SHIP_COLOR[dir] };
    EntityOptions o;
    o.shape          = Span<const Str>(shape, 1);
    o.color          = Span<const Str>(color, 1);
    o.x              = x;
    o.z              = DEPTH_WATER_GAP1;
    o.args           = true;
    o.dx             = speed;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.auto_trans     = true;
    anim->new_entity(o);
}

// The frames are built rather than written out: five idle, then the seven
// spouts, each indented to the blowhole.
void add_whale(Entity *, Animation *anim)
{
    i32 dir         = rand_int(2);
    f64 speed       = 0.5;
    i32 x           = -18;
    i32 spout_align = 11;
    if (dir == 1) {
        speed       = -0.5;
        x           = anim->Width() - 2;
        spout_align = 1;
    }

    String frames[12];
    for (i32 i = 0; i < 5; i++)
        if (!frames[i].append("\n\n\n") || !frames[i].append(WHALE_SHAPE[dir]))
            return;
    String sep;
    if (!sep.push('\n'))
        return;
    for (i32 k = 0; k < spout_align; k++)
        if (!sep.push(' '))
            return;
    for (i32 s = 0; s < 7; s++) {
        String &f = frames[5 + s];
        Str rest  = WATER_SPOUTS[s];
        for (bool first = true;; first = false) {
            usize i  = rest.find('\n');
            Str head = i == Str::npos ? rest : rest.substr(0, i);
            if (!first && !f.append(sep.str()))
                return;
            if (!f.append(head))
                return;
            if (i == Str::npos)
                break;
            rest = rest.substr(i + 1);
        }
        if (!f.push('\n') || !f.append(WHALE_SHAPE[dir]))
            return;
    }

    Str shape[12];
    Str color[12];
    for (i32 i = 0; i < 12; i++) {
        shape[i] = frames[i].str();
        color[i] = WHALE_COLOR[dir];
    }
    EntityOptions o;
    o.shape          = Span<const Str>(shape, 12);
    o.color          = Span<const Str>(color, 12);
    o.x              = x;
    o.z              = DEPTH_WATER_GAP2;
    o.args           = true;
    o.dx             = speed;
    o.frame_step     = 1;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.auto_trans     = true;
    anim->new_entity(o);
}

void add_monster(Entity *, Animation *anim)
{
    if (rand_int(2) == 0)
        add_new_monster(anim);
    else
        add_old_monster(anim);
}

// Upstream's weighting: design two two thirds of the time.
void add_big_fish(Entity *, Animation *anim)
{
    if (rand_int(3) > 0)
        add_big_fish2(anim);
    else
        add_big_fish1(anim);
}

// Three parts sharing one callback, so the rig moves together.
void add_fishhook(Entity *, Animation *anim)
{
    i32 x       = 10 + rand_int(max_int(1, anim->Width() - 30));
    i32 y_start = -20;
    i32 y_line  = y_start + HOOK_LINE_DY;

    String line;
    if (!str_repeat(line, "|\n", HOOK_LINE_HEIGHT))
        return;
    Str line_shape[1] = { line.str() };
    EntityOptions line_opts;
    line_opts.type       = ET_FISHLINE;
    line_opts.shape      = Span<const Str>(line_shape, 1);
    line_opts.x          = x + 7;
    line_opts.y          = y_line;
    line_opts.z          = DEPTH_WATER_LINE1;
    line_opts.callback   = fishhook_callback;
    line_opts.mode       = HOOK_LOWERING;
    line_opts.auto_trans = true;
    anim->new_entity(line_opts);
    Str hook_shape[1] = { HOOK_SHAPE };
    EntityOptions hook_opts;
    hook_opts.type           = ET_FISHHOOK;
    hook_opts.shape          = Span<const Str>(hook_shape, 1);
    hook_opts.x              = x;
    hook_opts.y              = y_start;
    hook_opts.z              = DEPTH_WATER_LINE1;
    hook_opts.callback       = fishhook_callback;
    hook_opts.mode           = HOOK_LOWERING;
    hook_opts.death_callback = fishhook_death;
    hook_opts.default_color  = COLOR_GREEN;
    hook_opts.auto_trans     = true;
    anim->new_entity(hook_opts);
    Str point_shape[1] = { HOOK_POINT_SHAPE };
    EntityOptions point_opts;
    point_opts.type          = ET_HOOK_POINT;
    point_opts.shape         = Span<const Str>(point_shape, 1);
    point_opts.x             = x + 1;
    point_opts.y             = y_start + HOOK_POINT_DY;
    point_opts.z             = DEPTH_SHARK + 1;
    point_opts.callback      = fishhook_callback;
    point_opts.mode          = HOOK_LOWERING;
    point_opts.default_color = COLOR_GREEN;
    point_opts.physical      = true;
    anim->new_entity(point_opts);
}

// Switches an entity to the upward movement after a catch.
void retract(Entity *e, Animation *)
{
    e->physical = false;
    if (e->type == ET_FISH) {
        e->z        = f64(DEPTH_WATER_GAP2);
        e->callback = fishhook_callback;
        e->mode     = HOOK_HOOKED;
        return;
    }
    e->mode = HOOK_HOOKED;
    if (e->type == ET_FISHHOOK)
        e->die_offscreen = true;
}

void add_ducks(Entity *, Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 1.0;
    i32 x     = -30;
    if (dir == 1) {
        speed = -1;
        x     = anim->Width() - 2;
    }
    Str color[1] = { DUCK_COLOR[dir] };
    EntityOptions o;
    o.shape          = Span<const Str>(DUCK_SHAPE[dir], 3);
    o.color          = Span<const Str>(color, 1);
    o.x              = x;
    o.y              = 5;
    o.z              = DEPTH_WATER_GAP3;
    o.args           = true;
    o.dx             = speed;
    o.frame_step     = 0.25;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.auto_trans     = true;
    anim->new_entity(o);
}

// Three, spaced fifteen apart; only the lead chains the next event.
void add_dolphins(Entity *, Animation *anim)
{
    i32 dir      = rand_int(2);
    f64 speed    = 2.0;
    i32 x        = -13;
    i32 distance = 15;
    if (dir == 1) {
        speed    = -2;
        x        = anim->Width() - 2;
        distance = -15;
    }
    Str color[1] = { DOLPHIN_COLOR[dir] };
    for (i32 i = 0; i < 3; i++) {
        u8 default_color = COLOR_CYAN;
        if (i == 0)
            default_color = COLOR_BLUE;
        else if (i == 1)
            default_color = COLOR_MAGENTA;
        EntityOptions o;
        o.type           = ET_DOLPHIN;
        o.shape          = Span<const Str>(DOLPHIN_SHAPE[dir], 2);
        o.color          = Span<const Str>(color, 1);
        o.x              = x - (distance * (2 - i));
        o.y              = 5;
        o.z              = DEPTH_WATER_GAP3;
        o.callback       = dolphin_delay_offscreen_death;
        o.args           = true;
        o.dx             = speed;
        o.frame_step     = 0.5;
        o.death_callback = i == 0 ? random_object : nullptr;
        o.default_color  = default_color;
        o.auto_trans     = true;
        anim->new_entity(o);
    }
}

void add_swan(Entity *, Animation *anim)
{
    i32 dir   = rand_int(2);
    f64 speed = 1.0;
    i32 x     = -10;
    if (dir == 1) {
        speed = -1;
        x     = anim->Width() - 2;
    }
    Str shape[1] = { SWAN_SHAPE[dir] };
    Str color[1] = { SWAN_COLOR[dir] };
    EntityOptions o;
    o.shape          = Span<const Str>(shape, 1);
    o.color          = Span<const Str>(color, 1);
    o.x              = x;
    o.y              = 1;
    o.z              = DEPTH_WATER_GAP3;
    o.args           = true;
    o.dx             = speed;
    o.frame_step     = 0.25;
    o.die_offscreen  = true;
    o.death_callback = random_object;
    o.auto_trans     = true;
    anim->new_entity(o);
}

// The event router. Nearly every special's death calls it, so events chain.
void random_object(Entity *dead, Animation *anim)
{
    static constexpr EntityDeathHandler CHOICES[] = {
        add_ship,     add_whale, add_monster, add_big_fish, add_shark,
        add_fishhook, add_swan,  add_ducks,   add_dolphins,
    };
    CHOICES[rand_int(i32(sizeof CHOICES / sizeof CHOICES[0]))](dead, anim);
}
