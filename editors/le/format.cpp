/*
 * Copyright (c) 1993-2005 by Alexander V. Lukyanov (lav@yars.free.net)
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

/*_____________________________________________________________________________
**
**  File:           format.cc
**  Description:    Format functions for text editor
**_____________________________________________________________________________
*/
#include "format.h"

#include "config.h"
#include "edit.h"
#include "keymap.h"
#include "undo.h"
#ifdef HAVE_ALLOCA_H
#endif

int LineLen         = 63;
int LeftMargin      = 0;
int FirstLineMargin = 3;
int LeftAdj         = 1;
int RightAdj        = 0;
int wordwrap        = 0;

Task<void> FormatPara()
{
    num bcol, ncol;
    int i;

    if (in_hex_mode || View || buffer_mmapped) /* formatting is not allowed in those modes */
        co_return;

    flag = 1;
    ToLineBegin();
    for (;;) {
        int space = 0;
        while (Space() && !Eof()) {
            ExpandTab();
            MoveRightOverEOL();
            space++;
        }
        if (Eof())
            co_return;
        if (Eol()) {
            DeleteBlock(space, 0);
            space = 0;
            MoveRightOverEOL();
        } else
            break;
    }

    /* fold the paragraph, that is delete all spaces but one and all newlines */
    for (;;) {
        while (!Space() && !Eol())
            MoveRightOverEOL();
        while (Space() && !Eol())
            DeleteChar();
        if (Eol()) {
            DeleteEOL();
            for (i = ncol = 0; !Eol() && Space(); i++) {
                if (Char() == '\t')
                    ncol = Tabulate(ncol);
                else
                    ncol += MBCharWidth;
                MoveRightOverEOL();
            }
            // end of paragraph condition:
            //    empty line, or
            //    large left margin (in case we ajust left)
            if (Eol() || (ncol > LeftMargin && LeftAdj)) {
                while (i-- > 0)
                    MoveLeftOverEOL();
                break; // end of paragraph
            }
            while (i-- > 0)
                BackSpace(); /* delete old indentation */
        }
        InsertChar(' ');
    }

    NewLine();
    SetStdCol();
    MoveUp();

    if (LeftAdj) {
        /* create the first line margin */
        for (i = ncol = 0; ncol < LeftMargin + FirstLineMargin && Space() && !Eol(); i++) {
            if (Char() == '\t')
                ncol = Tabulate(ncol);
            else
                ncol += MBCharWidth;
            MoveRightOverEOL();
        }
        if (ncol < LeftMargin + FirstLineMargin) {
            while (!Bol())
                BackSpace();
            for (i = FirstLineMargin + LeftMargin; i > 0; i--)
                InsertChar(' ');
        } else {
            while (ncol < LeftMargin + LineLen / 2 && Space() && !Eol()) {
                if (Char() == '\t')
                    ncol = Tabulate(ncol);
                else
                    ncol += MBCharWidth;
                MoveRightOverEOL();
            }
            while (Space() && !Eol())
                DeleteChar();
        }
    } else {
        while (Space() && !Eol())
            DeleteChar();
    }

    for (;;) {
        if (GetCol() > LineLen + LeftMargin) {
            /* if the next word is over limit, then ... */
            while (!Bol() && !SpaceLeft())
                MoveLeftOverEOL();
            while (!Bol() && SpaceLeft()) /* one word right */
                MoveLeftOverEOL();
            if (Bol()) {
                stdcol = 0; /* the word consumes the whole line */
                while (!Eol() && Space())
                    MoveRightOverEOL();
                while (!Eol() && !Space())
                    MoveRightOverEOL();
                if (!Eol()) {
                    DeleteChar(); /* delete space after the word */
                    NewLine();
                    continue;
                }
                break;
            } else
                DeleteChar();
            bcol = GetCol();

            assert(GetCol() <= LeftMargin + LineLen);

            if (RightAdj && LeftAdj) {
                /* insert spaces to extend the line to the width */

                int gap_num          = 0;
                int spaces_to_insert = LineLen + LeftMargin - bcol;
                int i;

                if (spaces_to_insert > 0) {
                    /* To the line beginning, and count spaces */
                    while (!Bol()) {
                        MoveLeftOverEOL();
                        if (Space())
                            gap_num++;
                    }
                    /* skip indentation */
                    while (Space() && GetCol() < bcol) {
                        MoveRightOverEOL();
                        gap_num--;
                    }
                    i = -gap_num / 2;
                    while (GetCol() < bcol) {
                        if (Space()) {
                            MoveRightOverEOL();
                            i += spaces_to_insert;
                            while (i > 0) {
                                InsertChar(' ');
                                bcol++;
                                i -= gap_num;
                            }
                        } else {
                            MoveRightOverEOL();
                        }
                    }
                }
                NewLine();
            } else if (!LeftAdj && RightAdj) {
                ToLineBegin();
                while (bcol < LineLen + LeftMargin) {
                    InsertChar(' ');
                    bcol++;
                }
                while (GetCol() < bcol)
                    MoveRightOverEOL();
                NewLine();
            } else if (!LeftAdj && !RightAdj) {
                while (GetCol() < bcol)
                    MoveRightOverEOL();
                NewLine();
                MoveUp();
                co_await CenterLine();
                MoveDown();
            } else {
                NewLine();
            }
            for (i = LeftMargin; i > 0; i--)
                InsertChar(' ');
        } else {
            if (Eol())
                break; /* this is the end of line and the paragraph */
            MoveRightOverEOL();
        }
    }
    if (!LeftAdj && RightAdj) {
        bcol = GetCol();
        ToLineBegin();
        while (bcol < LineLen + LeftMargin) {
            InsertChar(' ');
            bcol++;
        }
        while (GetCol() < bcol)
            MoveRightOverEOL();
    }

    ToLineBegin();
    SetStdCol();
    MoveDown();
}

Task<void> FormatAll()
{
    static struct menu FAmenu[] = { { "   &Ok   ", MIDDLE - 6, FDOWN - 2 },
                                    { " &Cancel ", MIDDLE + 6, FDOWN - 2 },
                                    { NULL } };

    if (in_hex_mode || View)
        co_return;

    char message[80];
    strcpy(message, "ALL text will be formatted");
    if (!undo.Enabled())
        strcat(message, " (no undo)");

    switch (co_await ReadMenuBox(FAmenu, HORIZ, message, " Verify ", VERIFY_WIN_ATTR,
                                 CURR_BUTTON_ATTR)) {
    case (0):
    case ('C'):
        co_return;
    }
    MessageSync("Formatting all document...");
    TextPoint oldpos = CurrentPos;
    CurrentPos       = TextBegin;
    while (!Eof())
        co_await FormatPara();
    CurrentPos = oldpos;
}

Task<void> CenterLine()
{
    num shift;
    if (in_hex_mode || View)
        co_return;
    flag = REDISPLAY_LINE;
    ToLineBegin();
    while (Space() && !Eol())
        DeleteChar();
    if (Eol())
        co_return; /* nothing to center */
    ToLineEnd();
    while (SpaceLeft())
        BackSpace();
    shift = (LineLen - GetCol()) / 2 + LeftMargin;
    if (shift > 0) /* not too long line */
    {
        ToLineBegin();
        while (shift--)
            InsertChar(' ');
    }
    ToLineBegin();
    SetStdCol();
}

Task<void> ShiftRightLine()
{
    num shift;
    if (in_hex_mode || View)
        co_return;
    flag = REDISPLAY_LINE;
    ToLineBegin();
    while (Space() && !Eol())
        DeleteChar();
    if (Eol())
        co_return; /* nothing to shift */
    ToLineEnd();
    while (SpaceLeft())
        BackSpace();
    shift = (LineLen - GetCol()) + LeftMargin;
    if (shift > 0) /* not too long line */
    {
        ToLineBegin();
        while (shift--)
            InsertChar(' ');
    }
    ToLineBegin();
    SetStdCol();
}

Task<void> FormatFunc()
{
    int action;

    if (in_hex_mode || View)
        co_return;
    ToLineBegin();
    SetStdCol();
again:
    ClearMessage();
    CenterView();
    SyncTextWin();
    Message("Format: F-Format all P-format Paragraph C-Center line R-align Right");
    SetCursor();

    action = co_await GetNextAction();
    switch (action) {
    case (LINE_UP):
        co_await UserLineUp();
        goto again;
    case (LINE_DOWN):
        co_await UserLineDown();
        goto again;
    case (REFRESH_SCREEN):
        co_await UserRefreshScreen();
        break;
    default:
        if (StringTypedLen != 1)
            break;
        switch (StringTyped[0]) {
        case ('P'):
        case ('p'):
            MessageSync("Formatting one paragraph...");
            co_await FormatPara();
            RedisplayAll();
            goto again;
        case ('R'):
        case ('r'): {
            MessageSync("Shifting right...");
            co_await ShiftRightLine();
            co_await UserLineDown();
            RedisplayLine();
            goto again;
        }
        case ('C'):
        case ('c'):
            MessageSync("Centering...");
            co_await CenterLine();
            co_await UserLineDown();
            RedisplayLine();
            goto again;
        case ('F'):
        case ('f'):
            co_await FormatAll();
            flag = REDISPLAY_ALL;
            break;
        }
    }
}

void WordWrapInsertHook()
{
    if (GetCol() < LineLen + LeftMargin)
        return;
    offs pos = CurrentPos;
    while (!BolAt(pos) && !SpaceLeftAt(pos))
        pos--;
    if (pos == CurrentPos)
        return;
    offs word_begin = pos;
    while (!BolAt(pos) && SpaceLeftAt(pos))
        pos--;
    if (BolAt(pos))
        return; // it was the first word on the line
    TextPoint old(CurrentPos);
    CurrentPos = word_begin;
    DeleteBlock(word_begin - pos, 0);
    NewLine();
    for (int i = LeftMargin; i > 0; i--)
        InsertChar(' ');
    CurrentPos = old;
    SetStdCol();
}
