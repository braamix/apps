/*
 * The screen, and the keyboard.
 *
 * This replaces ex_tty.c, which read a terminal's description out of the
 * termcap database, and the half of ex_put.c that drove one. Braam has no
 * terminal type and no escape sequences: the screen is an array of cells that
 * the renderer reads straight out of linear memory, so a capability is a
 * constant (ex_screen.h) and cursor addressing is indexing.
 *
 * Visual mode is not built yet. What is here is the part command mode needs:
 * the standout flag, and the claim-release dance around a shell escape.
 */
#include "ex.h"
#include "ex_screen.h"
#include "ex_vis.h"

ProcScreen *vscreen;

/*
 * {code, mods} onto the byte codes vi's decoder expects. There are no control
 * characters on this keyboard -- ^C is 'c' with the control modifier set --
 * so this is where they are made.
 *
 * The arrows answer ^P, ^N, ^H and space rather than k, j, h and l, because
 * those four work in insert mode too and hjkl would type themselves.
 */
int key_byte(Key k)
{
    if (k.mods & MOD_CTRL) {
        if (k.code >= 'a' && k.code <= 'z')
            return ((int)k.code - 'a' + 1);
        if (k.code >= 'A' && k.code <= 'Z')
            return ((int)k.code - 'A' + 1);
        if (k.code == '[')
            return (ESCAPE);
        if (k.code == '\\')
            return (QUIT);
        if (k.code == '?')
            return (DELETE);
        return (0);
    }
    switch (k.code) {
    case KEY_ESCAPE:
        return (ESCAPE);
    case KEY_ENTER:
        return ('\r');
    case KEY_TAB:
        return ('\t');
    case KEY_BACKSPACE:
        return ('\b');
    case KEY_DELETE:
        return ('x'); /* vi has no forward delete */
    case KEY_UP:
        return (CTRL(p));
    case KEY_DOWN:
        return (CTRL(n));
    case KEY_LEFT:
        return ('\b');
    case KEY_RIGHT:
        return (' ');
    case KEY_HOME:
        return ('^');
    case KEY_END:
        return ('$');
    case KEY_PAGE_UP:
        return (CTRL(b));
    case KEY_PAGE_DOWN:
        return (CTRL(f));
    }
    /*
     * ex is a byte editor: TRIM is 0177 throughout, and a line is a char
     * array. A codepoint above ASCII has nowhere to go.
     */
    return (k.code < 0x80 ? (int)k.code : 0);
}

/*
 * Give the screen and the keyboard back, so that a shell escape's child can
 * take them, and take them again afterwards.
 *
 * The order is load-bearing and is the shell's own (sh/job.cpp): the keyboard
 * goes back *before* anything is spawned, because a full-screen program claims
 * it in its very first step and would otherwise lose the race.
 */
Task<void> vspawn_begin(void)
{
    if (vscreen == 0)
        co_return;
    co_await vscreen_give();
}

Task<void> vspawn_end(void)
{
    if (vscreen == 0)
        co_return;
    if (Task<Result<void>> t = vscreen_take())
        co_await t;
}

Task<Result<void>> vscreen_take(void)
{
    co_return {};
}

Task<void> vscreen_give(void)
{
    co_return;
}

/*
 * Visual mode is not built yet. Command mode reaches a handful of its
 * routines -- clearing the echo area, noting a change inside a macro -- and
 * each of them checks inopen first, which is never set while there is no
 * visual to set it.
 */
void vclrech(exbool didphys)
{
    (void)didphys;
}

Task<void> vmacchng(exbool fromvis)
{
    (void)fromvis;
    co_return;
}

Task<void> vop(void)
{
    COTHROW(error("Visual is not built in this binary"));
}

/*
 * The rest of what command mode reaches into visual for. Every one of these
 * call sites is guarded by inopen, which nothing sets while there is no
 * visual mode to set it, so none of them runs; they are here because the
 * call sites are upstream's and stay.
 */
void vclear(void)
{
}

void vclrbyte(char *cp, int cnt)
{
    (void)cp;
    (void)cnt;
}

void vclreol(void)
{
}

void vclean(void)
{
}

void vgoto(int y, int x)
{
    (void)y;
    (void)x;
}

void vmoveitup(int cnt, exbool doclr)
{
    (void)cnt;
    (void)doclr;
}

void vreplace(int l, int cnt, int newcnt)
{
    (void)l;
    (void)cnt;
    (void)newcnt;
}

void vsetsiz(int i)
{
    (void)i;
}

void fixech(void)
{
}

void undvis(void)
{
}

exbool ateopr(void)
{
    return (1);
}

Task<int> getkey(void)
{
    co_return (ESCAPE);
}

void ungetkey(int c)
{
    (void)c;
}
