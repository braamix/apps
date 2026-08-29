/*
 * Copyright (c) 1993-2013 by Alexander V. Lukyanov (lav@yars.free.net)
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
#include "mb.h"

Task<int> choose_ch()
{
    WIN *w;
    int i, j;
    static int curr = 0;
    int res         = -1;
    char s[256];
    char chstr[MB_CUR_MAX + 2];
    int action;

    w = CreateWin(MIDDLE, MIDDLE, 3 * 16 + 4, 16 + 6, DIALOGUE_WIN_ATTR, " Character Set ", 0);
    DisplayWin(w);

    for (;;) {
        SetAttr(DIALOGUE_WIN_ATTR);
        Clear();

        if (curr < 32)
            snprintf(chstr, sizeof(chstr), "^%c", curr + '@');
        else {
            if (0) {
                int w = wcwidth(curr);
                if (w == 0)
                    chstr[0] = ' '; // to show accents nicely.
                int ch_len = wctomb(chstr + (w == 0), curr);
                if (ch_len < 0)
                    ch_len = 0;
                chstr[ch_len + (w == 0)] = 0;
            } else // note the following line
                snprintf(chstr, sizeof(chstr), "%c", curr);
        }
        snprintf(s, sizeof(s), "The current character is '%s', %3d, 0%03o, 0x%02X", chstr, curr,
                 curr, curr);

        PutStr(2, 2, s);
        for (i = 0; i < 16; i++)
            for (j = 0; j < 16; j++) {
                SetAttr((i << 4) + j == curr ? CURR_BUTTON_ATTR : DIALOGUE_WIN_ATTR);
                PutCh(i * 3 + 2, j + 4, ' ');
                if (0)
                    PutWCh(i * 3 + 3, j + 4, (i << 4) + j);
                else // note the next line
                    PutCh(i * 3 + 3, j + 4, (i << 4) + j);
                PutCh(i * 3 + 4, j + 4, ' ');
            }
        action = co_await GetNextAction();
        switch (action) {
        case (NEWLINE):
            res = curr;
            goto done;
        case (CANCEL):
            res = -1;
            goto done;
        case (LINE_UP):
            if ((curr & 15) == 0)
                curr |= 15;
            else
                curr--;
            break;
        case (LINE_DOWN):
            if ((curr & 15) == 15)
                curr &= ~15;
            else
                curr++;
            break;
        case (CHAR_LEFT):
            if ((curr & 0xF0) == 0)
                curr |= 0xF0;
            else
                curr -= 16;
            break;
        case (CHAR_RIGHT):
            if ((curr & 0xF0) == 0xF0)
                curr &= ~0xF0;
            else
                curr += 16;
            break;
        }
    }
done:
    CloseWin();
    DestroyWin(w);
    co_return (res);
}
Task<wchar_t> choose_wch()
{
    WIN *w;
    int i, j;
    static wchar_t curr = 0;
    wchar_t res         = -1;
    char s[256];
    char chstr[MB_CUR_MAX + 2];
    int action;

    w = CreateWin(MIDDLE, MIDDLE, 3 * 16 + 4, 16 + 6, DIALOGUE_WIN_ATTR, " Character Set ", 0);
    DisplayWin(w);

    for (;;) {
        SetAttr(DIALOGUE_WIN_ATTR);
        Clear();

        if (curr / 256)
            PutStr(FRIGHT - 3, FDOWN, " PgUp/PgDn ");
        else
            PutStr(FRIGHT - 6, FDOWN, " PgDn ");

        if (curr < 32)
            snprintf(chstr, sizeof(chstr), "^%c", curr + '@');
        else {
            int w = wcwidth(curr);
            if (w == 0)
                chstr[0] = ' '; // to show accents nicely.
            int ch_len = wctomb(chstr + (w == 0), curr);
            if (ch_len < 0)
                ch_len = 0;
            chstr[ch_len + (w == 0)] = 0;
        }
        snprintf(s, sizeof(s), "The current character is '%s', 0x%04X", chstr, curr);

        PutStr(2, 2, s);
        for (i = 0; i < 16; i++)
            for (j = 0; j < 16; j++) {
                SetAttr((i << 4) + j == curr % 256 ? CURR_BUTTON_ATTR : DIALOGUE_WIN_ATTR);
                PutCh(i * 3 + 2, j + 4, ' ');
                PutWCh(i * 3 + 3, j + 4, (i << 4) + j + (curr & ~255));
                PutCh(i * 3 + 4, j + 4, ' ');
            }

        action = co_await GetNextAction();
        switch (action) {
        case (NEWLINE):
            res = curr;
            goto done;
        case (CANCEL):
            res = -1;
            goto done;
        case (LINE_UP):
            if ((curr & 15) == 0)
                curr |= 15;
            else
                curr--;
            break;
        case (LINE_DOWN):
            if ((curr & 15) == 15)
                curr &= ~15;
            else
                curr++;
            break;
        case (CHAR_LEFT):
            if ((curr & 0xF0) == 0)
                curr |= 0xF0;
            else
                curr -= 16;
            break;
        case (CHAR_RIGHT):
            if ((curr & 0xF0) == 0xF0)
                curr &= ~0xF0;
            else
                curr += 16;
            break;
        case (NEXT_PAGE):
            curr += 256;
            break;
        case (PREV_PAGE):
            if (curr < 256)
                break;
            curr -= 256;
            break;
        }
    }
done:
    CloseWin();
    DestroyWin(w);
    co_return (res);
}

void addch_visual(chtype ch)
{
    attrset(curr_attr->n_attr);
    if (ch & A_ALTCHARSET)
        addch(ch);
    else {
        unsigned char ct = ch & A_CHARTEXT;
        if (!iswprint(ct)) {
            if (ct < 32)
                ct += '@';
            else if (ct == 127)
                ct = '?';
            else
                ct = '.';
            attrset(curr_attr->so_attr);
            addch(ct);
            attrset(curr_attr->n_attr);
        } else
            addch(ch);
    }
}

/* Upstream asked a user-editable bitmap which bytes were unprintable, because
   only the user knew the terminal's 8-bit encoding. A codepoint here says so
   itself: iswprint is C0, DEL and C1, and nothing else. */
wchar_t visualize_wchar(wchar_t wc)
{
    if (wc < 0 || iswprint(wc))
        return wc;
    if (wc < 32)
        return wc + '@';
    if (wc == 127)
        return '?';
    return '.';
}

/* What tables.cc held besides the D211 and VTA2000 keyboard maps.
 *
 * ModifyKey was the software Cyrillic layout: upstream read a Latin key and
 * looked up the Russian letter on the same physical key, because a terminal
 * could not tell it which one the user had meant. The browser sends the
 * codepoint that was typed, so there is nothing to translate. */

void InitModifyKeyTables()
{
}

int ModifyKey(int key)
{
    return key;
}
