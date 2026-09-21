#include "kernel/screen.h"

#include "clock.h"
#include "math/math.h"
#include "ui/grid.h"

int SCREENX = 80;
int SCREENY = 24;

s_simplechar LAYER[MAXLAYERS + 1][MAXX][MAXY];
s_simplechar FINAL[MAXX][MAXY];
s_simplechar WRITECHAR = { 32, 7, 8, false, false, false };
s_simplechar CLEARCHAR = { 32, 7, 8, true, true, true };

static u8 map_fg(unsigned short c)
{
    if (c >= 16)
        return u8((c - 8) | COLOR_BRIGHT);
    return u8(c & 7);
}

static u8 map_bg(unsigned short c)
{
    if (c >= C_BGTRANS)
        return u8(COLOR_BLACK);
    return u8(c & 7);
}

void initscreen(int cols, int rows)
{
    SCREENY = rows > MAXY ? MAXY - 1 : rows;
    SCREENX = cols > MAXX ? MAXX - 1 : cols;
}

void charxy(int layer, int x, int y, s_simplechar &simplechar)
{
    if (x >= 0 && x <= SCREENX && y >= 0 && y <= SCREENY)
        LAYER[layer][x][y] = simplechar;
}

void stringxy(int layer, int x, int y, s_simplechar &simplechar, const char *s)
{
    for (int i = 0; s[i]; i++) {
        simplechar.chr = s[i];
        charxy(layer, x + i, y, simplechar);
    }
}

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

void linexy(int layer, int x1, int y1, int x2, int y2, s_simplechar &simplechar)
{
    int t;
    if (iabs(x2 - x1) >= iabs(y2 - y1)) {
        float fy = float(y2 - y1) / float(x2 - x1 ? x2 - x1 : 1);
        if (x1 > x2) {
            t  = x1;
            x1 = x2;
            x2 = t;
            t  = y1;
            y1 = y2;
            y2 = t;
        }
        for (int x = x1; x <= x2; x++)
            charxy(layer, x, int(round(y1 + fy * (x - x1))), simplechar);
    } else {
        float fx = float(x2 - x1) / float(y2 - y1 ? y2 - y1 : 1);
        if (y1 > y2) {
            t  = x1;
            x1 = x2;
            x2 = t;
            t  = y1;
            y1 = y2;
            y2 = t;
        }
        for (int y = y1; y <= y2; y++)
            charxy(layer, int(round(x1 + fx * (y - y1))), y, simplechar);
    }
}

void clearlayer(int layer, s_simplechar &simplechar)
{
    for (int y = 0; y <= SCREENY; y++) {
        for (int x = 0; x <= SCREENX; x++)
            LAYER[layer][x][y] = simplechar;
    }
}

void clearalllayer(s_simplechar simplechar)
{
    clearlayer(0, WRITECHAR);
    for (int l = 1; l < MAXLAYERS; l++)
        clearlayer(l, simplechar);
}

void mergelayers()
{
    for (int l = 0; l <= MAXLAYERS; l++) {
        for (int y = 0; y <= SCREENY; y++) {
            for (int x = 0; x <= SCREENX; x++) {
                FINAL[x][y].chr = LAYER[l][x][y].transpchr ? FINAL[x][y].chr : LAYER[l][x][y].chr;
                FINAL[x][y].col = LAYER[l][x][y].transpcol ? FINAL[x][y].col : LAYER[l][x][y].col;
                FINAL[x][y].bcol =
                    LAYER[l][x][y].transpbcol ? FINAL[x][y].bcol : LAYER[l][x][y].bcol;
            }
        }
    }
}

void paint_final_to_grid(Grid &g)
{
    for (int y = 0; y < SCREENY && u32(y) < g.rows; y++) {
        for (int x = 0; x < SCREENX && u32(x) < g.cols; x++) {
            Cell *c = g.at(u32(x), u32(y));
            if (!c)
                continue;
            char32_t ch = FINAL[x][y].chr ? FINAL[x][y].chr : ' ';
            u8 fg       = map_fg(FINAL[x][y].col);
            u8 bg       = map_bg(FINAL[x][y].bcol);
            if (c->ch == ch && c->fg == fg && c->bg == bg)
                continue;
            c->ch    = ch;
            c->fg    = fg;
            c->bg    = bg;
            c->attrs = 0;
            g.touch(u32(x), u32(y), 1, 1);
        }
    }
}
