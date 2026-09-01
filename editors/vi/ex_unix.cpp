/* Copyright (c) 1979 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

#include "kernel/fmt.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * Unix escapes, filtering
 */

/*
 * First part of a shell escape,
 * parse the line, expanding # and % and ! and printing if implied.
 */
Task<void> unix0(exbool warn)
{
    char *up, *fp;
    short c;
    char printub, puxb[UXBSIZE + sizeof(int)];

    printub = 0;
    CP(puxb, uxb);
    c = getchar();
    if (c == '\n' || c == EOF)
        COTHROW(error("Incomplete shell escape command@- use 'shell' to get a shell"));
    up = uxb;
    do {
        switch (c) {
        case '\\':
            if (any(peekchar(), "%#!"))
                c = getchar();
        default:
            if (up >= &uxb[UXBSIZE]) {
            tunix:
                uxb[0] = 0;
                COTHROW(error("Command too long"));
            }
            *up++ = c;
            break;

        case '!':
            fp = puxb;
            if (*fp == 0) {
                uxb[0] = 0;
                COTHROW(error("No previous command@to substitute for !"));
            }
            printub++;
            while (*fp) {
                if (up >= &uxb[UXBSIZE])
                    goto tunix;
                *up++ = *fp++;
            }
            break;

        case '#':
            fp = altfile;
            if (*fp == 0) {
                uxb[0] = 0;
                COTHROW(error("No alternate filename@to substitute for #"));
            }
            goto uexp;

        case '%':
            fp = savedfile;
            if (*fp == 0) {
                uxb[0] = 0;
                COTHROW(error("No filename@to substitute for %%"));
            }
        uexp:
            printub++;
            while (*fp) {
                if (up >= &uxb[UXBSIZE])
                    goto tunix;
                *up++ = *fp++ | QUOTE;
            }
            break;
        }
        c = getchar();
    } while (c == '"' || c == '|' || !endcmd(c));
    if (c == EOF)
        ungetchar(c);
    *up = 0;
    if (!inopen)
        resetflav();
    if (warn)
        ckaw();
    if (warn && hush == 0 && chng && xchng != chng && value(WARN) && dol > zero) {
        xchng = chng;
        vnfl();
        printf(mesg("[No write]|[No write since last change]"));
        noonl();
        flush();
    } else
        warn = 0;
    if (printub) {
        if (uxb[0] == 0)
            COTHROW(error("No previous command@to repeat"));
        if (inopen) {
            splitw++;
            vclean();
            vgoto(WECHO, 0);
        }
        if (warn)
            vnfl();
        if (hush == 0)
            lprintf("!%s", uxb);
        if (inopen && Outchar != termchar) {
            vclreol();
            vgoto(WECHO, 0);
        } else
            putnl();
        flush();
    }
}

/*
 * Run a shell escape.
 *
 * Upstream forked, rearranged the child's descriptors, and exec'd the shell;
 * mode said whether a pipe was wanted on either side. Here spawn() takes the
 * descriptors as an argument, so the rearranging is the ChildIo, and the two
 * halves that had to be a fork -- the filter's input, which upstream wrote
 * from a *second copy of the editor* -- are temp files instead. One task
 * cannot park on two descriptors, so a filter driven through two pipes would
 * deadlock the moment either one filled.
 *
 * The claims have to go back before anything is spawned, and in this order:
 * the screen, then the keyboard, then the child, then the console. A
 * full-screen program claims the keyboard in its very first step, so a child
 * that races us for it loses (sh/job.cpp says so).
 */
static char in_name[32];
static char out_name[32];

static void tmpname(char *buf, char which)
{
    Buf<32> b;

    b.put("/tmp/vi").put(which).put('.').put(proc_pid());
    memcpy(buf, b.str().data(), b.str().size());
    buf[b.str().size()] = 0;
}

/*
 * The shell, and what it is told to run. Answers its exit status, or -1.
 */
static Task<int> runsh(char *opt, char *up, int fdin, int fdout)
{
    Str words[3];
    Args v;
    ChildIo cio;
    Result<u32> pid_r = Err(Error::NoMemory);
    u32 child;

    words[0] = Str(svalue(SHELL), strlen(svalue(SHELL)));
    words[1] = Str(opt, strlen(opt));
    words[2] = Str(up, strlen(up));
    v.v      = Span<const Str>(words, 3);
    cio.in   = fdin >= 0 ? (u32)fdin : SYS_STDIN;
    cio.out  = fdout >= 0 ? (u32)fdout : SYS_STDOUT;

    if (Task<Result<u32>> t = spawn(v, cio))
        pid_r = co_await t;
    if (pid_r.is_err()) {
        errno = errno_of(pid_r.error());
        co_return (-1);
    }
    child = res_of(pid_r);
    pid   = (int)child;

    /*
     * The console goes to the child so that ^C reaches it rather than the
     * editor; a program that has not claimed the keyboard can ask for this
     * only while it is itself in front.
     */
    if (Task<Result<void>> t = set_fg(child))
        co_await t;
    {
        Result<Exited> w = Err(Error::NoMemory);

        if (Task<Result<Exited>> t = wait_child(child))
            w = co_await t;
        if (Task<Result<void>> t = set_fg(0))
            co_await t;
        if (w.is_err()) {
            errno = errno_of(w.error());
            co_return (-1);
        }
        status = res_of(w).status;
        co_return (status);
    }
}

Task<void> unixex(char *opt, char *up, int newstdin, int mode)
{
    (void)mode;
    co_await runsh(opt, up, newstdin, -1);
    if (newstdin > 0)
        co_await b_close(newstdin);
}

/*
 * Wait for the command to complete.
 * C flags suppression of printing.
 *
 * The waiting is done in runsh, which has to do it anyway to give the console
 * back; what is left of this is the "!" and the redraw.
 */
Task<void> unixwt(exbool c, int p)
{
    (void)p;
    if (!inopen && c && hush == 0) {
        printf("!\n");
        flush();
        co_await exflush();
    }
}

/*
 * Set up the filtration implied by mode, which is like an open number: 1 means
 * the command's output replaces the range, 2 means the range is its input, 3
 * means both.
 *
 * Upstream built a pipe each way and forked a second editor to write the
 * range down the first one. Both ends of a pipeline cannot be driven from one
 * task here, so each side is a file in /tmp, which also makes the order
 * obvious: write the range, run the command, read the result back.
 */
Task<void> filter(int mode)
{
    int lines = lineDOL();
    int fdin = -1, fdout = -1;
    int saveio = io;

    mode++;
    tmpname(in_name, 'i');
    tmpname(out_name, 'o');

    if (mode & 2) {
        io = co_await b_creat(in_name, 0644);
        if (io < 0)
            COTHROW(filioerr(in_name));
        co_await putfile();
        COCHK;
        co_await b_close(io);
        io   = -1;
        fdin = co_await b_open(in_name, O_RDONLY);
        if (fdin < 0)
            COTHROW(filioerr(in_name));
    }
    if (mode & 1) {
        fdout = co_await b_creat(out_name, 0644);
        if (fdout < 0) {
            if (fdin >= 0)
                co_await b_close(fdin);
            COTHROW(filioerr(out_name));
        }
    }

    /*
     * The screen only has to be handed over when the child writes to it.
     * A filter's output goes to a file, so it never does, and giving the
     * alternate screen back and taking it again would cost a full repaint
     * of a buffer that is halfway through being changed.
     */
    if (fdout < 0)
        co_await vspawn_begin();
    co_await runsh((char *)"-c", uxb, fdin, fdout);
    if (fdout < 0) {
        co_await vcontin(1); /* the child wrote on the console; pause first */
        co_await vspawn_end(0);
    }

    if (fdout >= 0)
        co_await b_close(fdout);
    if (mode == 3) {
        exdelete(0);
        addr2 = addr1 - 1;
    }
    if (mode & 1) {
        if (FIXUNDO)
            undap1 = undap2 = addr2 + 1;
        io = co_await b_open(out_name, O_RDONLY);
        if (io < 0)
            COTHROW(filioerr(out_name));
        ignore(co_await append(getfile, addr2));
        co_await b_close(io);
    }
    io = saveio;
    if (mode & 2)
        co_await b_unlink(in_name);
    if (mode & 1)
        co_await b_unlink(out_name);
    co_await unixwt(!inopen, 0);
    netchHAD(lines);
}
