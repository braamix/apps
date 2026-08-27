/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * Set command.
 */
char optname[ONMSZ];

void set(void)
{
    char *cp;
    struct option *op;
    int c;
    exbool no;

    setnoaddr();
    if (skipend()) {
        if (peekchar() != EOF)
            ignchar();
        propts();
        return;
    }
    do {
        cp = optname;
        do {
            if (cp < &optname[ONMSZ - 2])
                *cp++ = getchar();
        } while (isalnum(peekchar()));
        *cp = 0;
        cp  = optname;
        if (eq("all", cp)) {
            if (inopen)
                pofix();
            prall();
            goto next;
        }
        no = 0;
        if (cp[0] == 'n' && cp[1] == 'o') {
            cp += 2;
            no++;
        }
        /*
         * Implement w300, w1200, and w9600 specially.
         * These chose a window size from the line's speed. There is no
         * line: the screen is an array both sides can see, so this is
         * always the fast case, and the slow two are ignored exactly as
         * they were on a terminal that was already fast.
         */
        if (eq(cp, "w300") || eq(cp, "w1200")) {
            ignore(getchar()); /* = */
            ignore(getnum());  /* value */
            continue;
        } else if (eq(cp, "w9600")) {
            cp = "window";
        }
        for (op = options; op < &options[NOPTS]; op++)
            if (eq(op->oname, cp) || op->oabbrev && eq(op->oabbrev, cp))
                break;
        if (op->oname == 0)
            THROW(serror("%s: No such option@- 'set all' gives all option values", cp));
        c = skipwh();
        if (peekchar() == '?') {
            ignchar();
        printone:
            propt(op);
            noonl();
            goto next;
        }
        if (op->otype == ONOFF) {
            op->ovalue = 1 - no;
            if (op == &options[PROMPT])
                oprompt = 1 - no;
            goto next;
        }
        if (no)
            THROW(serror("Option %s is not a toggle", op->oname));
        if (c != 0 || setend())
            goto printone;
        if (getchar() != '=')
            THROW(serror("Missing =@in assignment to option %s", op->oname));
        switch (op->otype) {
        case NUMERIC:
            if (!isdigit(peekchar()))
                THROW(error("Digits required@after ="));
            op->ovalue = getnum();
            if (value(TABSTOP) <= 0)
                value(TABSTOP) = TABS;
            if (op == &options[WINDOW]) {
                if (value(WINDOW) >= LINES)
                    value(WINDOW) = LINES - 1;
                vsetsiz(value(WINDOW));
            }
            break;

        case STRING:
        case OTERM:
            cp = optname;
            while (!setend()) {
                if (cp >= &optname[ONMSZ])
                    THROW(error("String too long@in option assignment"));
                /* adb change:  allow whitepace in strings */
                if ((*cp = getchar()) == '\\')
                    if (peekchar() != EOF)
                        *cp = getchar();
                cp++;
            }
            *cp = 0;
            if (op->otype == OTERM) {
                /*
                 * At first glance it seems like we shouldn't care if the terminal type
                 * is changed inside visual mode, as long as we assume the screen is
                 * a mess and redraw it. However, it's a much harder problem than that.
                 * If you happen to change from 1 crt to another that both have the same
                 * size screen, it's OK. But if the screen size if different, the stuff
                 * that gets initialized in vop() will be wrong. This could be overcome
                 * by redoing the initialization, e.g. making the first 90% of vop into
                 * a subroutine. However, the most useful case is where you forgot to do
                 * a setenv before you went into the editor and it thinks you're on a dumb
                 * terminal. Ex treats this like hardcopy and goes into HARDOPEN mode.
                 * This loses because the first part of vop calls oop in this case.
                 * The problem is so hard I gave up. I'm not saying it can't be done,
                 * but I am saying it probably isn't worth the effort.
                 */
                /*
                 * There is no terminal type to set: the screen
                 * is an array of cells, so a name would name
                 * nothing. :set term is answered, not obeyed.
                 */
                THROW(error("Terminal type is not settable"));
            } else {
                CP(op->osvalue, optname);
                op->odefault = 1;
            }
            break;
        }
    next:
        flush();
    } while (!skipend());
    eol();
}

exbool setend(void)
{
    return (iswhite(peekchar()) || endcmd(peekchar()));
}

void prall(void)
{
    int incr          = (NOPTS + 2) / 3;
    int rows          = incr;
    struct option *op = options;

    for (; rows; rows--, op++) {
        propt(op);
        tab(24);
        propt(&op[incr]);
        if (&op[2 * incr] < &options[NOPTS]) {
            tab(56);
            propt(&op[2 * incr]);
        }
        putNFL();
    }
}

void propts(void)
{
    struct option *op;

    for (op = options; op < &options[NOPTS]; op++) {
        if (op == &options[TTYTYPE])
            continue;
        switch (op->otype) {
        case ONOFF:
        case NUMERIC:
            if (op->ovalue == op->odefault)
                continue;
            break;

        case STRING:
            if (op->odefault == 0)
                continue;
            break;
        }
        propt(op);
        putchar(' ');
    }
    noonl();
    flush();
}

void propt(struct option *op)
{
    char *name;

    name = op->oname;

    switch (op->otype) {
    case ONOFF:
        printf("%s%s", op->ovalue ? "" : "no", name);
        break;

    case NUMERIC:
        printf("%s=%d", name, op->ovalue);
        break;

    case STRING:
    case OTERM:
        printf("%s=%s", name, op->osvalue);
        break;
    }
}
