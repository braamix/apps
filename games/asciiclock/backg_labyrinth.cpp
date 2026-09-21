#include "clock.h"
#include "math/math.h"

const int LABLAYER = 0;

// s_simplechar LABCLEAR = { 32, C_DGRAY, C_GRAY, false, false, false};
s_simplechar LABWALL         = { 35, C_LBLUE, C_BLUE, false, false, false };
s_simplechar LABEMPTY        = { 32, C_DGRAY, C_BLACK, false, false, false };
s_simplechar LABPLAYER       = { 111, C_LYELLOW, C_BLACK, false, false, false };
s_simplechar LABPLAYERWALKED = { 46, C_WHITE, C_BLUE, false, false, false };

struct s_direction {
    int x = 0;
    int y = 0;
};

s_direction DIRECTIONS[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };

int DIRSELECTOR[4] = { 0, 1, 2, 3 };

int LAB_PLAYERDIR = 0;

s_2dcoord LAB_PLAYER = { 1, 1 };

// Iterative backtracker: upstream recursed on wasm and blew the native stack.
struct LabStack {
    int x;
    int y;
    int next_dir;
};

static LabStack lab_stk[MAXX * MAXY / 4];
static int lab_top;

static void lab_shuffle_dirs()
{
    for (int i = 0; i < 4; i++) {
        int other          = clock_rand() % 4;
        int tmp            = DIRSELECTOR[i];
        DIRSELECTOR[i]     = DIRSELECTOR[other];
        DIRSELECTOR[other] = tmp;
    }
}

static bool lab_can_carve(int x, int y, int dir)
{
    int nx = x + DIRECTIONS[DIRSELECTOR[dir]].x * 2;
    int ny = y + DIRECTIONS[DIRSELECTOR[dir]].y * 2;
    if (nx <= 0 || nx >= SCREENX || ny <= 0 || ny >= SCREENY)
        return false;
    return LAYER[LABLAYER][nx][ny].chr == LABWALL.chr;
}

static void labyrinth_grid(int sx, int sy)
{
    lab_top            = 0;
    lab_stk[lab_top++] = { sx, sy, 0 };
    charxy(LABLAYER, sx, sy, LABEMPTY);

    while (lab_top > 0) {
        LabStack *f = &lab_stk[lab_top - 1];
        if (f->next_dir == 0)
            lab_shuffle_dirs();

        int carved = 0;
        for (; f->next_dir < 4; f->next_dir++) {
            if (!lab_can_carve(f->x, f->y, f->next_dir))
                continue;
            int mx = f->x + DIRECTIONS[DIRSELECTOR[f->next_dir]].x;
            int my = f->y + DIRECTIONS[DIRSELECTOR[f->next_dir]].y;
            int nx = f->x + DIRECTIONS[DIRSELECTOR[f->next_dir]].x * 2;
            int ny = f->y + DIRECTIONS[DIRSELECTOR[f->next_dir]].y * 2;
            charxy(LABLAYER, mx, my, LABEMPTY);
            charxy(LABLAYER, nx, ny, LABEMPTY);
            f->next_dir++;
            lab_stk[lab_top++] = { nx, ny, 0 };
            carved             = 1;
            break;
        }
        if (!carved)
            lab_top--;
    }
}

// initialize labyrinth
void init_bg_labyrinth()
{
    int x;
    clearalllayer(CLEARCHAR);
    clearlayer(LABLAYER, LABWALL);
    LAB_PLAYER.x = 1 + clock_rand() % (SCREENX - 3);
    LAB_PLAYER.y = 1 + clock_rand() % (SCREENY - 3);
    labyrinth_grid(1 + clock_rand() % (SCREENX - 3), 1 + clock_rand() % (SCREENY - 3));
    for (x = 0; x < SCREENX; x++) {
        charxy(LABLAYER, x, 0, LABWALL);
        charxy(LABLAYER, x, SCREENY - 1, LABWALL);
    }
    for (x = 0; x < SCREENY; x++) {
        charxy(LABLAYER, 0, x, LABWALL);
        charxy(LABLAYER, SCREENX - 1, x, LABWALL);
    }

    CLOCKCHAR.transpbcol = false;
    CLOCKCHAR.transpcol  = false;
    CLOCKCHAR.transpchr  = false;
    // clock color can't be black
    AVOIDCLOCKCOLOR = LABWALL.bcol;
}

// calc next frame
__attribute__((noinline)) void calc_bg_labyrinth()
{
    int freedirs = 0, direction = 0;
    charxy(LABLAYER, LAB_PLAYER.x, LAB_PLAYER.y, LABPLAYERWALKED);

    // check directions
    for (direction = 0; direction < 4; direction++) {
        // if dir is inside of screen
        if (LAB_PLAYER.x + DIRECTIONS[direction].x > 0 &&
            LAB_PLAYER.x + DIRECTIONS[direction].x < SCREENX &&
            LAB_PLAYER.y + DIRECTIONS[direction].y > 0 &&
            LAB_PLAYER.y + DIRECTIONS[direction].y < SCREENY) {
            // and is not explored yet
            if (LAYER[LABLAYER][LAB_PLAYER.x + DIRECTIONS[direction].x]
                     [LAB_PLAYER.y + DIRECTIONS[direction].y]
                         .chr == LABEMPTY.chr) {
                freedirs++;
            }
        }
    }
    // if there are unexplored directions
    if (freedirs > 0) {
        do {
            LAB_PLAYERDIR = (LAB_PLAYERDIR + 1) % 4; // choose a direction

        } while (LAYER[LABLAYER][LAB_PLAYER.x + DIRECTIONS[LAB_PLAYERDIR].x]
                      [LAB_PLAYER.y + DIRECTIONS[LAB_PLAYERDIR].y]
                          .chr == LABPLAYERWALKED.chr); // what is not explored

    } else { // if we are surrounded by already walked areas
        // and in this direction there's a wall
        if (LAYER[LABLAYER][LAB_PLAYER.x + DIRECTIONS[LAB_PLAYERDIR].x]
                 [LAB_PLAYER.y + DIRECTIONS[LAB_PLAYERDIR].y]
                     .chr == LABWALL.chr) {
            LAB_PLAYERDIR = clock_rand() % 4; // choose another dir
        }
    }

    // if in that direction there's no wall, then step there
    if (LAYER[LABLAYER][LAB_PLAYER.x + DIRECTIONS[LAB_PLAYERDIR].x]
             [LAB_PLAYER.y + DIRECTIONS[LAB_PLAYERDIR].y]
                 .chr != LABWALL.chr) {
        LAB_PLAYER.x = LAB_PLAYER.x + DIRECTIONS[LAB_PLAYERDIR].x;
        LAB_PLAYER.y = LAB_PLAYER.y + DIRECTIONS[LAB_PLAYERDIR].y;
    }

    // draw our little explorer char
    charxy(LABLAYER, LAB_PLAYER.x, LAB_PLAYER.y, LABPLAYER);
}

// not used
void draw_bg_labyrinth()
{
}