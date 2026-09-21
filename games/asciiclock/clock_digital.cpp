#include "clock.h"
#include "clock_digits.h"

const int CLOCKLAYER = 2;

int CLOCKCOLOR      = 0;
int AVOIDCLOCKCOLOR = 0;
int ACTDIGITDESIGN  = 2;

s_simplechar CLOCKCHAR  = { 32, 1, 2, true, true, false };
s_simplechar CLOCKCLEAR = { 32, 0, 8, true, true, true };

static bool colon = false;

void init_clock_digital()
{
    colon          = false;
    ACTDIGITDESIGN = clock_rand() % MAXDIGITDESIGNS;
    do {
        CLOCKCOLOR = clock_rand() % 8;
    } while (CLOCKCOLOR == AVOIDCLOCKCOLOR || CLOCKCOLOR == 0);
}

__attribute__((noinline)) static void draw_clock_digit(int layer, int px, int py, int digit,
                                                       s_simplechar chr)
{
    int design = ACTDIGITDESIGN % MAXDIGITDESIGNS;
    int dx     = int(DIGITDESIGNS[design].x);
    int dy     = int(DIGITDESIGNS[design].y);
    if (digit < 0 || digit > 11)
        return;
    for (int y = 0; y < dy; y++) {
        for (int x = 0; x < dx; x++) {
            if (CLOCKDIGIT8X8[design][digit][y][x] == 120)
                charxy(layer, px + x, py + y, chr);
        }
    }
}

__attribute__((noinline)) void draw_clock_digital(int cx, int cy)
{
    CLOCKCHAR.bcol = u16(CLOCKCOLOR);
    int dx         = int(DIGITDESIGNS[ACTDIGITDESIGN % MAXDIGITDESIGNS].x);
    clearlayer(CLOCKLAYER, CLOCKCLEAR);
    for (int digit = 0; digit < 2; digit++) {
        int d = ACT_TIMESTR[0][digit] - '0';
        if (d < 0 || d > 9)
            d = 0;
        draw_clock_digit(CLOCKLAYER, cx + (digit * dx), cy, d, CLOCKCHAR);
    }
    if (LAST_TIMESTR[0][0] != ACT_TIMESTR[0][0] || LAST_TIMESTR[0][1] != ACT_TIMESTR[0][1] ||
        LAST_TIMESTR[0][2] != ACT_TIMESTR[0][2] || LAST_TIMESTR[0][3] != ACT_TIMESTR[0][3] ||
        LAST_TIMESTR[0][4] != ACT_TIMESTR[0][4] || LAST_TIMESTR[0][5] != ACT_TIMESTR[0][5])
        colon = !colon;
    if (colon)
        draw_clock_digit(CLOCKLAYER, cx + (2 * dx), cy, 10, CLOCKCHAR);
    for (int digit = 2; digit < 4; digit++) {
        int d = ACT_TIMESTR[0][digit] - '0';
        if (d < 0 || d > 9)
            d = 0;
        draw_clock_digit(CLOCKLAYER, cx + dx + (digit * dx), cy, d, CLOCKCHAR);
    }
    for (int i = 0; i < 6; i++)
        LAST_TIMESTR[0][i] = ACT_TIMESTR[0][i];
}
