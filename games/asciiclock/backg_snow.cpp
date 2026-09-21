#include "clock.h"
#include "math/math.h"

// basic snow background

const int MAXSNOW = 100, // number of flakes
    MAXSPD        = 2,   // number of snowflake speed types - 0 1 2
    SNOWLAYER     = 1;   // where to draw snow

// snowflake characters -> . + *
s_simplechar SNOWCHARS[3] = { { 42, C_WHITE, 8, false, false, true },
                              { 43, C_GRAY, 8, false, false, true },
                              { 46, C_DGRAY, 8, false, false, true } };

// char for clearing the screen
s_simplechar SNOWCLEAR = { 32, 0, 8, false, false, false };

// max ground snow "density"
const int MAXSNOWBG = 5;

// ground snow datas - value represents density
int SNOWBG[MAXX][MAXY];

// snow characters on the ground  -  SNOWBG density -> _ . x X
s_simplechar SNOWBGCHARS[MAXSNOWBG] = { { 32, C_WHITE, 8, false, false,
                                          true }, // dummy char 0 is for nothing
                                        { 95, C_DGRAY, 8, false, false, true },
                                        { 46, C_GRAY, 8, false, false, true },
                                        { 120, C_GRAY, 8, false, false, true },
                                        { 88, C_WHITE, 8, false, false, true } };

// generic snow structure
struct s_snow {
    int x          = 0;
    int y          = 0;
    int spd        = 0;
    int spdcounter = 0;
};

// array to store the flakes' data
s_snow SNOWFLAKES[MAXSNOW];

// create a new snowflake in random position at top of the screen with random speed
void calc_new_flake(int i)
{
    SNOWFLAKES[i].x   = clock_rand() % SCREENX;
    SNOWFLAKES[i].y   = 0;
    SNOWFLAKES[i].spd = clock_rand() % (MAXSPD + 1);
}

// main initialization
void init_bg_snow()
{
    int i, x, y;
    // clear bg layers
    for (i = 0; i <= 1; i++) {
        clearlayer(i, SNOWCLEAR);
    }
    // generate new snowflakes
    for (i = 0; i < MAXSNOW; i++) {
        calc_new_flake(i);
        SNOWFLAKES[i].y =
            clock_rand() % SCREENY; // give them initial y coords instead of top of the screen
    }
    // clear the ground snow
    for (y = 0; y < SCREENY; y++) {
        for (x = 0; x <= SCREENX; x++) {
            SNOWBG[x][y] = 0;
        }
    }
    // draw the initial ground to have something to collide with ( the "_______" line)
    for (x = 0; x <= SCREENX; x++) {
        SNOWBG[x][SCREENY - 1] = 1;
    }
    // set clock transparency
    CLOCKCHAR.transpbcol = false;
    CLOCKCHAR.transpcol  = true;
    CLOCKCHAR.transpchr  = true;
    // clock color can't be black
    AVOIDCLOCKCOLOR = 0;
}

// increase density of the snow at the ground
void inc_snow_bg(int x, int y)
{
    if (x < 0 || x > SCREENX || y < 0 || y > SCREENY)
        return;
    SNOWBG[x][y]++;
    if (SNOWBG[x][y] > MAXSNOWBG - 1) {
        SNOWBG[x][y] = MAXSNOWBG - 1;
        if (y > 0) {
            SNOWBG[x][y - 1]++;
            if (SNOWBG[x][y - 1] > MAXSNOWBG - 1) {
                SNOWBG[x][y - 1] = MAXSNOWBG - 1;
            }
        }
    }
}

// calculate the new position of snowflakes
void calc_bg_snow()
{
    int i;
    for (i = 0; i < MAXSNOW; i++) {
        // increase the snowflake's counter (simulates the speed)
        SNOWFLAKES[i].spdcounter++;

        // if counter hits the spd -> move snowflake
        if (SNOWFLAKES[i].spdcounter > SNOWFLAKES[i].spd + 2) {
            SNOWFLAKES[i].spdcounter = 0;
            SNOWFLAKES[i].y++; // go down :)

            // if it reaches the bottom of the screen
            if (int(SNOWFLAKES[i].y) > SCREENY) {
                // generate a new flake at random position
                calc_new_flake(i);
            } else {
                if (int(SNOWFLAKES[i].x) > 0 && int(SNOWFLAKES[i].x) < SCREENX &&
                    int(SNOWFLAKES[i].y) > 0 && int(SNOWFLAKES[i].y) < SCREENY) {
                    // if it contacts a "ground" snow
                    if (SNOWBG[SNOWFLAKES[i].x][SNOWFLAKES[i].y] > 0) {
                        inc_snow_bg(SNOWFLAKES[i].x - 1, SNOWFLAKES[i].y);
                        inc_snow_bg(SNOWFLAKES[i].x, SNOWFLAKES[i].y);
                        inc_snow_bg(SNOWFLAKES[i].x + 1, SNOWFLAKES[i].y);

                        // we allow the snowflakes to flow to the bottom of the screen "behind" the
                        // onground snow it will create a more solid pattern :) with
                        // calc_new_flake(i) there will be gaps on the ground

                        // calc_new_flake(i);
                    }
                }
            }
            SNOWFLAKES[i].x += clock_rand() % 3 - 1;
            inc_snow_bg(5, SCREENY - 1);
        }
    }
}

//
void draw_bg_snow()
{
    int i, x, y;
    // clear the layer
    clearlayer(SNOWLAYER, SNOWCLEAR);
    // put the flakes to layer
    for (i = 0; i < MAXSNOW; i++) {
        charxy(1, SNOWFLAKES[i].x, SNOWFLAKES[i].y, SNOWCHARS[SNOWFLAKES[i].spd]);
    }
    // draw the snow to the ground
    for (y = 0; y <= SCREENY; y++) {
        for (x = 0; x <= SCREENX; x++) {
            if (SNOWBG[x][y] > 0) {
                charxy(SNOWLAYER, x, y, SNOWBGCHARS[SNOWBG[x][y]]);
            }
        }
    }
}
