/*
 * Copyright (c) 1993-2017 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "edit.h"
#include "keymap.h"

struct GraphChar {
    int ch;
    enum { L = 0, T, R, B };
    char prol[4];
};

/* Upstream carried five of these: a "dumb" ASCII approximation, three
   eight-bit Cyrillic codepages, and this. The renderer has the glyphs, so
   this is the only one that was ever chosen here and the only one left. */
struct GraphChar UnicodeCoding[] = {
    /* {'│',{0,1,0,1}},{'┤',{1,1,0,1}},{'╡',{2,1,0,1}},{'╢',{1,2,0,2}},
       {'╖',{1,0,0,2}},{'╕',{2,0,0,1}},{'╣',{2,2,0,2}},{'║',{0,2,0,2}},
       {'╗',{2,0,0,2}},{'╝',{2,2,0,0}},{'╜',{1,2,0,0}},{'╛',{2,1,0,0}},
       {'┐',{1,0,0,1}},{'└',{0,1,1,0}},{'┴',{1,1,1,0}},{'┬',{1,0,1,1}},
       {'├',{0,1,1,1}},{'─',{1,0,1,0}},{'┼',{1,1,1,1}},{'╞',{0,1,2,1}},
       {'╟',{0,2,1,2}},{'╚',{0,2,2,0}},{'╔',{0,0,2,2}},{'╩',{2,2,2,0}},
       {'╦',{2,0,2,2}},{'╠',{0,2,2,2}},{'═',{2,0,2,0}},{'╬',{2,2,2,2}},
       {'╧',{2,1,2,0}},{'╨',{1,2,1,0}},{'╤',{2,0,2,1}},{'╥',{1,0,1,2}},
       {'╙',{0,2,1,0}},{'╘',{0,1,2,0}},{'╒',{0,0,2,1}},{'╓',{0,0,1,2}},
       {'╫',{1,2,1,2}},{'╪',{2,1,2,1}},{'┘',{1,1,0,0}},{'┌',{0,0,1,1}},
       {' ',{0,0,0,0}},{0,{0,0,0,0}} */
    { 0x2502, { 0, 1, 0, 1 } }, { 0x2524, { 1, 1, 0, 1 } }, { 0x2561, { 2, 1, 0, 1 } },
    { 0x2562, { 1, 2, 0, 2 } }, { 0x2556, { 1, 0, 0, 2 } }, { 0x2555, { 2, 0, 0, 1 } },
    { 0x2563, { 2, 2, 0, 2 } }, { 0x2551, { 0, 2, 0, 2 } }, { 0x2557, { 2, 0, 0, 2 } },
    { 0x255D, { 2, 2, 0, 0 } }, { 0x255C, { 1, 2, 0, 0 } }, { 0x255B, { 2, 1, 0, 0 } },
    { 0x2510, { 1, 0, 0, 1 } }, { 0x2514, { 0, 1, 1, 0 } }, { 0x2534, { 1, 1, 1, 0 } },
    { 0x252C, { 1, 0, 1, 1 } }, { 0x251C, { 0, 1, 1, 1 } }, { 0x2500, { 1, 0, 1, 0 } },
    { 0x253C, { 1, 1, 1, 1 } }, { 0x255E, { 0, 1, 2, 1 } }, { 0x255F, { 0, 2, 1, 2 } },
    { 0x255A, { 0, 2, 2, 0 } }, { 0x2554, { 0, 0, 2, 2 } }, { 0x2569, { 2, 2, 2, 0 } },
    { 0x2566, { 2, 0, 2, 2 } }, { 0x2560, { 0, 2, 2, 2 } }, { 0x2550, { 2, 0, 2, 0 } },
    { 0x256C, { 2, 2, 2, 2 } }, { 0x2567, { 2, 1, 2, 0 } }, { 0x2568, { 1, 2, 1, 0 } },
    { 0x2564, { 2, 0, 2, 1 } }, { 0x2565, { 1, 0, 1, 2 } }, { 0x2559, { 0, 2, 1, 0 } },
    { 0x2558, { 0, 1, 2, 0 } }, { 0x2552, { 0, 0, 2, 1 } }, { 0x2553, { 0, 0, 1, 2 } },
    { 0x256B, { 1, 2, 1, 2 } }, { 0x256A, { 2, 1, 2, 1 } }, { 0x2518, { 1, 1, 0, 0 } },
    { 0x250C, { 0, 0, 1, 1 } }, { ' ', { 0, 0, 0, 0 } },    { 0, { 0, 0, 0, 0 } }
};

struct GraphChar *CurrGraphSet;

int CharIndex(int ch)
{
    int i;
    for (i = 0; CurrGraphSet[i].ch && CurrGraphSet[i].ch != ch; i++)
        ;
    return i;
}
int DirIndex(int x, int y)
{
    return (x == 0 ? y + 2 : x + 1);
}
void TurnXY(int *x, int *y)
{
    int x1 = *y;
    *y     = -*x;
    *x     = x1;
}
int HowProlonged(int ch, int x, int y)
{
    return (CurrGraphSet[CharIndex(ch)].prol[DirIndex(x, y)]);
}

int IsProlonged(int ch, int x, int y, byte how)
{
    int dir = DirIndex(x, y);

    for (int i = 0; CurrGraphSet[i].ch; i++)
        if (ch == CurrGraphSet[i].ch && how == CurrGraphSet[i].prol[dir])
            return (1);

    return (how == 0);
}

void Prolong(int x, int y, byte how)
{
    struct GraphChar need;
    int best;
    int bestdist;
    int dist;
    int i;
    int j;

    need                      = CurrGraphSet[CharIndex(WChar())];
    need.prol[DirIndex(x, y)] = how;

    for (i = 1, TurnXY(&x, &y); i < 4; i++, TurnXY(&x, &y)) {
        int ch                    = WCharAtLC(GetLine() + y, GetCol() + x);
        need.prol[DirIndex(x, y)] = HowProlonged(ch, -x, -y);
    }

    best     = -1;
    bestdist = 30000;
    for (i = 0; CurrGraphSet[i].ch; i++) {
        j    = DirIndex(x, y);
        dist = 16 * abs(need.prol[j] - CurrGraphSet[i].prol[j]);
        j    = (j + 1) & 3;
        dist += 4 * abs(need.prol[j] - CurrGraphSet[i].prol[j]);
        j = (j + 1) & 3;
        dist += abs(need.prol[j] - CurrGraphSet[i].prol[j]);
        j = (j + 1) & 3;
        dist += 4 * abs(need.prol[j] - CurrGraphSet[i].prol[j]);

        if (dist < bestdist) {
            bestdist = dist;
            best     = i;
        }
    }
    PreUserEdit();
    ReplaceWCharExt(CurrGraphSet[best].ch);
}

void Draw(int x, int y, byte how)
{
    if (IsProlonged(WChar(), x, y, how)) {
        if (GetCol() < -x || GetLine() < -y)
            return;
        Prolong(x, y, how);
        if (y)
            RedisplayLine();
        HardMove(GetLine() + y, GetCol() + x);
        Prolong(-x, -y, how);
    } else
        Prolong(x, y, how);
    RedisplayLine();
}

Task<void> DrawFrames(void)
{
    static int curr_frame = 1;
    int action;
    char sign[4];

    if (in_hex_mode || View)
        co_return;

    CurrGraphSet = UnicodeCoding;

    do {
        ClearMessage();
        SyncTextWin();
        attrset(STATUS_LINE_ATTR->n_attr);
        snprintf(sign, sizeof(sign), " $%d", curr_frame);
        mvaddstr(StatusLineY, COLS - 4, sign);
        SetCursor();

        action = co_await GetNextAction();
        switch (action) {
        case (EDITOR_HELP):
            co_await Help("FramesHelp", " Frame-Drawing Help ");
            break;
        case (CHAR_LEFT):
            Draw(-1, 0, curr_frame);
            break;
        case (CHAR_RIGHT):
            Draw(1, 0, curr_frame);
            break;
        case (LINE_UP):
            Draw(0, -1, curr_frame);
            break;
        case (LINE_DOWN):
            Draw(0, 1, curr_frame);
            break;
        case (REFRESH_SCREEN):
            co_await UserRefreshScreen();
            break;
        default:
            if (StringTypedLen != 1) {
                beep();
                break;
            }
            switch (StringTyped[0]) {
            case ('\t'):
                curr_frame = (curr_frame == 2) ? 0 : curr_frame + 1;
                break;
            default:
                beep();
                co_return;
            }
        }
    } while (1);
}
