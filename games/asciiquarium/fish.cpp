// fish.go: the fish catalogue, their bubbles and the splat.

#include "quarium.h"

namespace {

// Left and right sprites, and the masks that go with them.
struct FishDesign {
    Str shape[2];
    Str color[2];
};

constexpr FishDesign OLD_FISH[] = {
    { { R"AQ(       \
     ...\..,
\  /'       \
 >=     (  ' >
/  \      / /
    `"'"'/'')AQ",
        R"AQ(      /
  ,../...
 /       '\  /
< '  )     =<
 \ \      /  \
  `'"'"')AQ" },
      { R"AQ(       2
     1112111
6  11       1
 66     7  4 5
6  1      3 1
    11111311)AQ",
        R"AQ(      2
  1112111
 1       11  6
5 4  7     66
 1 3      1  6
  11311111)AQ" } },
    { { R"AQ(    \
\ /--\
>=  (o>
/ \__/
    /)AQ",
        R"AQ(  /
 /--\ /
<o)  =<
 \__/ \
  \)AQ" },
      { R"AQ(    2
6 1111
66  745
6 1111
    3)AQ",
        R"AQ(  2
 1111 6
547  66
 1111 6
  3)AQ" } },
    { { R"AQ(       \:.
\;,   ,;\\\\,,
  \\\\;;:::::::o
  ///;;::::::::<
 /;` ``/////``)AQ",
        R"AQ(      .:/
   ,,///;,   ,;/
 o:::::::;;///
>::::::::;;\\\\
  ''\\\\\\\\'' ';\)AQ" },
      { R"AQ(       222
666   1122211
  6661111111114
  66611111111115
 666 113333311)AQ",
        R"AQ(      222
   1122211   666
 4111111111666
51111111111666
  113333311 666)AQ" } },
    { { R"AQ(  __
><_'>
   ')AQ",
        R"AQ( __
<'_><
 `)AQ" },
      { R"AQ(  11
61145
   3)AQ",
        R"AQ( 11
54116
 3)AQ" } },
    { { R"AQ(   ..\\
>='   ('>
  '''/'')AQ",
        R"AQ(  ,..
<')   `=<
 ``\```)AQ" },
      { R"AQ(   1121
661   745
  111311)AQ",
        R"AQ(  1211
547   166
 113111)AQ" } },
    { { R"AQ(   \
  / \
>=_('>
  \_/
   /)AQ",
        R"AQ(  /
 / \
<')_=<
 \_/
  \)AQ" },
      { R"AQ(   2
  1 1
661745
  111
   3)AQ",
        R"AQ(  2
 1 1
547166
 111
  3)AQ" } },
    { { R"AQ(  ,\
>=('>
  '/)AQ",
        R"AQ( /,
<')=<
 \`)AQ" },
      { R"AQ(  12
66745
  13)AQ",
        R"AQ( 21
54766
 31)AQ" } },
    { { R"AQ(  __
\/ o\
/\__/)AQ",
        R"AQ( __
/o \/
\__/\)AQ" },
      { R"AQ(  11
61 41
61111)AQ",
        R"AQ( 11
14 16
11116)AQ" } },
};

constexpr FishDesign NEW_FISH[] = {
    { { R"AQ(   \
  / \
>=_('>
  \_/
   /)AQ",
        R"AQ(  /
 / \
<')_=<
 \_/
  \)AQ" },
      { R"AQ(   1
  1 1
663745
  111
   3)AQ",
        R"AQ(  2
 111
547366
 111
  3)AQ" } },
    { { R"AQ(     ,
     }\\
\  .'  `\
}}<   ( 6>
/  `,  .'
     }/
     ')AQ",
        R"AQ(    ,
   /{
 /'  `.  /
<6 )   >{{
 `.  ,'  \
   {\
    `)AQ" },
      { R"AQ(     2
     22
6  11  11
661   7 45
6  11  11
     33
     3)AQ",
        R"AQ(    2
   22
 11  11  6
54 7   166
 11  11  6
   33
    3)AQ" } },
    { { R"AQ(            \'`.
             )  \
(`.      _.-`' ' '`-.
 \ `.  .`        (o) \_
  >  ><     (((       (
 / .`  ._      /_|  /'
(.`       `-. _  _.-`
            /__/')AQ",
        R"AQ(       .'`/
      /  (
  .-'` ` `'-._      .')
_/ (o)        '.  .' /
)       )))     ><  <
`\  |_\      _.'  '. \
  '-._  _ .-'       '.)
      `\__\)AQ" },
      { R"AQ(            1111
             1  1
111      11111 1 1111
 1 11  11        141 11
  1  11     777       5
 1 11  111      333  11
111       111 1  1111
            11111)AQ",
        R"AQ(       1111
      1  1
  1111 1 11111      111
11 141        11  11 1
5       777     11  1
11  333      111  11 1
  1111  1 111       111
      11111)AQ" } },
    { { R"AQ(       ,--,_
__    _\.---'-.
\ '.-"     // o\
/_.'-._    \\  /
       `"--(/"`)AQ",
        R"AQ(    _,--,
 .-'---./_    __
/o \\     "-.' /
\  //    _.-'._\
 `"\)--"`)AQ" },
      { R"AQ(       22222
66    121111211
6 6111     77 41
6661111    77  1
       11113311)AQ",
        R"AQ(    22222
 112111121    66
14 77     1116 6
1  77    1111666
 11331111)AQ" } },
};

constexpr Str SPLAT[4] = {
    R"AQ(

   .
  ***
   '

)AQ",
    R"AQ(

 .,*;`
 '*,**
 *'~'

)AQ",
    R"AQ(
  , ,
 " ,"'
 *" *'"
  " ; .

)AQ",
    R"AQ(* ' , ' `
' ` * . '
 ' `' ",'
* ' " * .
" * ', ')AQ",
};

constexpr Str BUBBLE[5] = { ".", "o", "O", "O", "O" };

constexpr char MASK_COLORS[] = "cCrRyYbBgGmM";

} // namespace

// Digits 1..9 become random colour letters, one draw a digit, so every 1 in a
// fish is the same colour.
bool rand_color(String &out, Str mask)
{
    char pick[10];
    for (i32 i = 1; i <= 9; i++)
        pick[i] = MASK_COLORS[rand_int(12)];
    out.clear();
    for (usize i = 0; i < mask.size(); i++) {
        char c = mask[i];
        if (c >= '1' && c <= '9')
            c = pick[c - '0'];
        if (!out.push(c))
            return false;
    }
    return true;
}

// One rising bubble, from the mouth side.
void add_bubble(Entity *fish, Animation *anim)
{
    i32 bx = fish->at_x();
    if (fish->dx > 0)
        bx += fish->width;
    EntityOptions o;
    o.type          = ET_BUBBLE;
    o.shape         = Span<const Str>(BUBBLE, 5);
    o.x             = bx;
    o.y             = fish->at_y() + fish->height / 2;
    o.z             = i32(fish->z) - 1;
    o.args          = true;
    o.dy            = -1;
    o.frame_step    = 0.1;
    o.die_offscreen = true;
    o.default_color = COLOR_CYAN;
    o.physical      = true;
    o.coll_handler  = bubble_collision;
    anim->new_entity(o);
}

// A bubble ends at the water line.
void bubble_collision(Entity *bubble, Animation *)
{
    for (Entity *obj : bubble->collision)
        if (obj->type == ET_WATERLINE) {
            bubble->kill();
            return;
        }
}

bool fish_callback(Entity *fish, Animation *anim)
{
    if (rand_int(100) + 1 > 97)
        add_bubble(fish, anim);
    return fish->move_entity();
}

// Teeth eat a small fish; a hook point retracts the whole rig with it.
void fish_collision(Entity *fish, Animation *anim)
{
    for (Entity *obj : fish->collision) {
        if (obj->type == ET_TEETH) {
            if (fish->height <= 5) {
                add_splat(anim, obj->at_x(), obj->at_y(), i32(obj->z));
                fish->kill();
            }
            return;
        }
        if (obj->type == ET_HOOK_POINT && obj->physical) {
            retract(obj, anim);
            retract(fish, anim);
            Vec<Entity *> found;
            anim->entities_by_type(ET_FISHHOOK, found);
            for (Entity *h : found)
                retract(h, anim);
            anim->entities_by_type(ET_FISHLINE, found);
            for (Entity *l : found)
                retract(l, anim);
            return;
        }
    }
}

// die_frame is frame steps, not seconds.
void add_splat(Animation *anim, i32 x, i32 y, i32 z)
{
    EntityOptions o;
    o.shape         = Span<const Str>(SPLAT, 4);
    o.x             = x - 4;
    o.y             = y - 2;
    o.z             = z - 2;
    o.args          = true;
    o.frame_step    = 0.25;
    o.die_frame     = 15;
    o.default_color = COLOR_RED;
    o.auto_trans    = true;
    anim->new_entity(o);
}

// Direction picks the speed's sign and the side it enters from. Its death
// spawns another, so the population holds.
void add_fish(Entity *, Animation *anim)
{
    const FishDesign *design;
    if (anim->classic || rand_int(12) + 1 <= 8)
        design = &OLD_FISH[rand_int(i32(sizeof OLD_FISH / sizeof OLD_FISH[0]))];
    else
        design = &NEW_FISH[rand_int(i32(sizeof NEW_FISH / sizeof NEW_FISH[0]))];

    i32 direction = rand_int(2);
    f64 speed     = 0.25 + rand_f64() * 1.75;
    if (direction == 1)
        speed *= -1;
    i32 depth = DEPTH_FISH_START + rand_int(DEPTH_FISH_END - DEPTH_FISH_START + 1);

    String mask;
    if (!rand_color(mask, design->color[direction]))
        return;

    Str shape[1] = { design->shape[direction] };
    Str color[1] = { mask.str() };
    EntityOptions o;
    o.type           = ET_FISH;
    o.shape          = Span<const Str>(shape, 1);
    o.color          = Span<const Str>(color, 1);
    o.z              = depth;
    o.callback       = fish_callback;
    o.args           = true;
    o.dx             = speed;
    o.die_offscreen  = true;
    o.death_callback = add_fish;
    o.physical       = true;
    o.coll_handler   = fish_collision;
    o.auto_trans     = true;
    Entity *fish     = anim->new_entity(o);
    if (!fish)
        return;

    i32 water_bottom  = 9;
    i32 screen_bottom = anim->Height() - 1;
    i32 available     = screen_bottom - water_bottom - fish->height;
    if (available > 0)
        fish->y = f64(water_bottom + rand_int(available + 1));
    else
        fish->y = f64(water_bottom);
    fish->x = direction == 0 ? f64(-fish->width) : f64(anim->Width());
}

// The initial count, from the screen's area; /350 is a density constant.
void add_all_fish(Animation *anim)
{
    i32 screen_size = (anim->Height() - 9) * anim->Width();
    i32 count       = screen_size / 350;
    if (count < 1)
        count = 1;
    for (i32 i = 0; i < count; i++)
        add_fish(nullptr, anim);
}
