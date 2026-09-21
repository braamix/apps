// ASCII clock — Deybacsi/asciiclock, ported to Braam.
// https://github.com/Deybacsi/asciiclock

#pragma once

#include "kernel/types.h"

constexpr float SCREEN_CHAR_ASPECT_RATIO = 2.0f;

constexpr int FPS          = 25;
constexpr int FPS2MILLISEC = 1000 / FPS;

constexpr short MAXLAYERS = 5;
// Upstream used 200; wasm initial memory fits 80×80 layer buffers.
constexpr int MAXX = 80;
constexpr int MAXY = 80;

constexpr int MAXTIMEZONES = 1;

constexpr int C_BLACK    = 0;
constexpr int C_RED      = 1;
constexpr int C_GREEN    = 2;
constexpr int C_YELLOW   = 3;
constexpr int C_BLUE     = 4;
constexpr int C_MAGENTA  = 5;
constexpr int C_CYAN     = 6;
constexpr int C_GRAY     = 7;
constexpr int C_DGRAY    = 8;
constexpr int C_LRED     = 9;
constexpr int C_LGREEN   = 10;
constexpr int C_LYELLOW  = 11;
constexpr int C_LBLUE    = 12;
constexpr int C_LMAGENTA = 13;
constexpr int C_LCYAN    = 14;
constexpr int C_WHITE    = 15;
constexpr int C_BGTRANS  = 8;

constexpr double PI = 3.14159265;

struct s_simplechar {
    char chr            = 32;
    unsigned short col  = 0;
    unsigned short bcol = 8;
    bool transpchr      = true;
    bool transpcol      = true;
    bool transpbcol     = true;
};

struct s_2dcoord {
    int x = 0;
    int y = 0;
};

struct s_3dcoord {
    float x = 0;
    float y = 0;
    float z = 0;
};

struct Grid; // kernel/screen.h

extern int SCREENX;
extern int SCREENY;

extern s_simplechar LAYER[MAXLAYERS + 1][MAXX][MAXY];
extern s_simplechar FINAL[MAXX][MAXY];
extern s_simplechar WRITECHAR;
extern s_simplechar CLEARCHAR;

extern int ACT_HOUR[MAXTIMEZONES];
extern int ACT_MIN[MAXTIMEZONES];
extern int ACT_SEC[MAXTIMEZONES];
extern char ACT_TIMESTR[MAXTIMEZONES][7];
extern char ACT_MINSTR[MAXTIMEZONES][5];
extern char LAST_TIMESTR[MAXTIMEZONES][7];
extern char LAST_MINSTR[MAXTIMEZONES][5];

extern int ACTDIGITDESIGN;
extern int CLOCKCOLOR;
extern int AVOIDCLOCKCOLOR;
extern s_simplechar CLOCKCHAR;

extern int ACT_BG_EFFECT;
extern int ACT_FG_EFFECT;

extern float izx;
extern float izy;

int clock_rand();
void clock_srand(u32 seed);

void initscreen(int cols, int rows);
void charxy(int layer, int x, int y, s_simplechar &simplechar);
void stringxy(int layer, int x, int y, s_simplechar &simplechar, const char *s);
void linexy(int layer, int x1, int y1, int x2, int y2, s_simplechar &simplechar);
void clearlayer(int layer, s_simplechar &simplechar);
void clearalllayer(s_simplechar simplechar);
void mergelayers();

s_2dcoord c3dto2d(s_3dcoord c3d);
s_3dcoord c3drotate(int axis, int angle, s_3dcoord c3d);

void init_clock_digital();
void draw_clock_digital(int cx, int cy);

void init_all();

typedef void (*EffectFn)();

extern EffectFn background[][3];
extern EffectFn foreground[][3];

extern const int BG_EFFECTNO;
extern const int FG_EFFECTNO;

void init_bg_snow();
void calc_bg_snow();
void draw_bg_snow();
void init_bg_star();
void calc_bg_star();
void draw_bg_star();
void init_bg_star3d();
void calc_bg_star3d();
void draw_bg_star3d();
void init_bg_plasma();
void calc_bg_plasma();
void draw_bg_plasma();
void init_bg_matrix();
void calc_bg_matrix();
void draw_bg_matrix();
void init_bg_fire();
void calc_bg_fire();
void draw_bg_fire();
void init_bg_labyrinth();
void calc_bg_labyrinth();
void draw_bg_labyrinth();
void init_bg_gof();
void calc_bg_gof();
void draw_bg_gof();
void init_bg_obj3d();
void calc_bg_obj3d();
void draw_bg_obj3d();
void init_fg_cube();
void calc_fg_cube();
void draw_fg_cube();

void paint_final_to_grid(Grid &g);
