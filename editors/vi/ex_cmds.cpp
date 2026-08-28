/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"
#include "ex_argv.h"
#include "ex_buf.h"
#include "ex_screen.h"

exbool pflag, nflag;
int poffset;

#define nochng() lchng = chng

/*
 * Main loop for command mode command decoding.
 * A few commands are executed here, but main function
 * is to strip command addresses, do a little address oriented
 * processing and call command routines to do the real work.
 */
Task<int> commands(exbool noprompt, exbool exitoneof)
{
    line *addr;
    int c;
    int lchng;
    int given;
    int seensemi;
    int cnt;
    exbool hadpr;

    resetflav();
    nochng();
    for (;;) {
        /*
         * The landing. Upstream arrived here by longjmp from error();
         * nothing unwinds by itself now, so the flag says whether the
         * last command ended badly, and ex_reset() does what error1()
         * did on the way: throw away the rest of that command's line.
         */
        if (ex_thrown) {
            if (ex_quitting)
                co_return (ex_status);
            /*
             * Under a visual :, the landing is excatch() and not this loop --
             * upstream threw to vreslab rather than resetlab. Staying would
             * read stdin for the next command, and in visual the keyboard is
             * a claim rather than stdin, so it would never answer.
             */
            if (vcatch)
                co_return (0);
            ex_reset();
        }
        if (ex_pendclose > 0) {
            co_await ex_close(ex_pendclose);
            ex_pendclose = -1;
        }

        /*
         * If dot at last command
         * ended up at zero, advance to one if there is a such.
         */
        if (dot <= zero) {
            dot = zero;
            if (dol > zero)
                dot = one;
        }
        shudclob = 0;

        /*
         * If autoprint or trailing print flags,
         * print the line at the specified offset
         * before the next command.
         */
        if (pflag || lchng != chng && value(AUTOPRINT) && !inglobal && !inopen && endline) {
            pflag = 0;
            nochng();
            if (dol != zero) {
                addr1 = addr2 = dot + poffset;
                if (addr1 < one || addr1 > dol)
                    THROWC(error("Offset out-of-bounds|Offset after command too large"));
                setdot1();
                if (ex_thrown)
                    continue;
                goto print;
            }
        }
        nochng();

        /*
         * Print prompt if appropriate.
         * If not in global flush output first to prevent
         * going into pfast mode unreasonably.
         */
        if (inglobal == 0) {
            flush();
            if (!hush && value(PROMPT) && !globp && !noprompt && endline) {
                putchar(':');
                hadpr = 1;
            }
        }

        /*
         * A whole line of input, before any of it is parsed. getach()
         * answers EOF rather than waiting, so the line has to be here
         * in full: a command stops at its newline and never asks for
         * the one after it.
         */
        if (need_input()) {
            co_await exflush();
            if ((co_await ex_readline()).is_err())
                co_return (0);
        }

        /*
         * Gobble up the address.
         * Degenerate addresses yield ".".
         */
        addr2 = 0;
        given = seensemi = 0;
        do {
            addr1 = addr2;
            addr  = address(0);
            c     = getcd();
            if (addr == 0)
                if (c == ',')
                    addr = dot;
                else if (addr1 != 0) {
                    addr2 = dot;
                    break;
                } else
                    break;
            addr2 = addr;
            given++;
            if (c == ';') {
                c        = ',';
                dot      = addr;
                seensemi = 1;
            }
        } while (c == ',');
        if (c == '%') {
            /* %: same as 1,$ */
            addr1 = one;
            addr2 = dol;
            given = 2;
            c     = getchar();
        }
        if (ex_thrown)
            continue;
        if (addr1 == 0)
            addr1 = addr2;
        if (c == ':')
            c = getchar();

        /*
         * Set command name for special character commands.
         */
        tailspec(c);

        /*
         * If called via : escape from open or visual, limit
         * the set of available commands here to save work below.
         */
        if (inopen) {
            if (c == '\n' || c == '\r' || c == CTRL('d') || c == EOF) {
                if (addr2)
                    dot = addr2;
                if (c == EOF)
                    co_return (0);
                continue;
            }
            if (any(c, "o"))
            notinvis:
                tailprim(Command, 1, 1);
        }
    choice:
        if (ex_thrown)
            continue;

        switch (c) {
        case 'a':

            switch (peekchar()) {
            case 'b':
                /* abbreviate */
                tail("abbreviate");
                setnoaddr();
                if (ex_thrown)
                    continue;
                mapcmd(0, 1);
                anyabbrs = 1;
                continue;
            case 'r':
                /* args */
                tail("args");
                setnoaddr();
                if (ex_thrown)
                    continue;
                eol();
                if (ex_thrown)
                    continue;
                pargs();
                continue;
            }

            /* append */
            if (inopen)
                goto notinvis;
            tail("append");
            setdot();
            if (ex_thrown)
                continue;
            aiflag = exclam();
            newline();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            deletenone();
            setin(addr2);
            inappend = 1;
            ignore(co_await append(gettty, addr2));
            inappend = 0;
            nochng();
            continue;

        case 'c':
            switch (peekchar()) {
                /* copy */
            case 'o':
                tail("copy");
                co_await vmacchng(0);
                co_await move();
                continue;

                /* cd */
            case 'd':
                tail("cd");
                goto changdir;

                /* chdir */
            case 'h':
                ignchar();
                if (peekchar() == 'd') {
                    char *p;
                    tail2of("chdir");
                changdir:
                    if (savedfile[0] == '/' || !value(WARN))
                        ignore(exclam());
                    else
                        ignore(quickly());
                    if (skipend()) {
                        p = getenv("HOME");
                        if (p == NULL)
                            THROWC(error("Home directory unknown"));
                    } else
                        getone(), p = file;
                    eol();
                    if (ex_thrown)
                        continue;
                    {
                        Result<String> r = Err(Error::NoMemory);

                        if (Task<Result<String>> t = cwd_set(Str(p, strlen(p))))
                            r = co_await t;
                        if (r.is_err()) {
                            errno = int(r.error());
                            THROWC(filioerr(p));
                        }
                    }
                    if (savedfile[0] != '/')
                        edited = 0;
                    continue;
                }
                if (inopen)
                    tailprim("change", 2, 1);
                tail2of("change");
                break;

            default:
                if (inopen)
                    goto notinvis;
                tail("change");
                break;
            }
            /* change */
            aiflag = exclam();
            setCNL();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            setin(addr1);
            exdelete(0);
            inappend = 1;
            ignore(co_await append(gettty, addr1 - 1));
            inappend = 0;
            nochng();
            continue;

            /* exdelete */
        case 'd':
            /*
             * Caution: dp and dl have special meaning already.
             */
            /* The command name, not exdelete()'s: tailprim() matches on it. */
            tail("delete");
            c = cmdreg();
            setCNL();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            if (c)
                YANKreg(c);
            exdelete(0);
            appendnone();
            continue;

            /* edit */
            /* ex */
        case 'e':
            tail((char *)(peekchar() == 'x' ? "ex" : "edit"));
        editcmd:
            if (!exclam() && chng)
                c = 'E';
            filename(c);
            if (c == 'E') {
                ungetchar(lastchar());
                ignore(quickly());
            }
            setnoaddr();
            if (ex_thrown)
                continue;
        doecmd:
            init();
            addr2 = zero;
            laste++;
            sync();
            co_await rop(c);
            nochng();
            continue;

            /* file */
        case 'f':
            tail("file");
            setnoaddr();
            if (ex_thrown)
                continue;
            filename(c);
            noonl();
            /*
                                    synctmp();
            */
            continue;

            /* global */
        case 'g':
            tail("global");
            co_await global(!exclam());
            nochng();
            continue;

            /* insert */
        case 'i':
            if (inopen)
                goto notinvis;
            tail("insert");
            setdot();
            if (ex_thrown)
                continue;
            nonzero();
            if (ex_thrown)
                continue;
            aiflag = exclam();
            newline();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            deletenone();
            setin(addr2);
            inappend = 1;
            ignore(co_await append(gettty, addr2 - 1));
            inappend = 0;
            if (dot == zero && dol > zero)
                dot = one;
            nochng();
            continue;

            /* join */
        case 'j':
            tail("join");
            c = exclam();
            setcount();
            if (ex_thrown)
                continue;
            nonzero();
            if (ex_thrown)
                continue;
            newline();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            if (given < 2 && addr2 != dol)
                addr2++;
            co_await join(c);
            continue;

            /* k */
        case 'k':
        casek:
            pastwh();
            c = getchar();
            if (endcmd(c))
                THROWC(serror("Mark what?|%s requires following letter", Command));
            newline();
            if (ex_thrown)
                continue;
            if (!islower(c))
                THROWC(error("Bad mark|Mark must specify a letter"));
            setdot();
            if (ex_thrown)
                continue;
            nonzero();
            if (ex_thrown)
                continue;
            names[c - 'a'] = *addr2 & ~01;
            anymarks       = 1;
            continue;

            /* list */
        case 'l':
            tail("list");
            setCNL();
            if (ex_thrown)
                continue;
            ignorf(setlist(1));
            pflag = 0;
            goto print;

        case 'm':
            if (peekchar() == 'a') {
                ignchar();
                if (peekchar() == 'p') {
                    /* map */
                    tail2of("map");
                    setnoaddr();
                    if (ex_thrown)
                        continue;
                    mapcmd(0, 0);
                    continue;
                }
                /* mark */
                tail2of("mark");
                goto casek;
            }
            /* move */
            tail("move");
            co_await vmacchng(0);
            co_await move();
            continue;

        case 'n':
            if (peekchar() == 'u') {
                tail("number");
                goto numberit;
            }
            /* next */
            tail("next");
            setnoaddr();
            if (ex_thrown)
                continue;
            ckaw();
            ignore(quickly());
            if (getargs())
                makargs();
            co_await next();
            c = 'e';
            filename(c);
            goto doecmd;

            /* open */
        case 'o':
            tail("open");
            THROWC(error("Open mode is not supported - use visual"));
            pflag = 0;
            nochng();
            continue;

        case 'p':
        case 'P':
            switch (peekchar()) {
                /* put */
            case 'u':
                tail("put");
                setdot();
                if (ex_thrown)
                    continue;
                c = cmdreg();
                eol();
                if (ex_thrown)
                    continue;
                co_await vmacchng(0);
                if (c)
                    co_await putreg(c);
                else
                    co_await put();
                continue;

            case 'r':
                ignchar();
                if (peekchar() == 'e') {
                    /* preserve */
                    /*
                     * Preserve wrote the temp file somewhere
                     * a setuid helper could find it after a
                     * crash. There is no temp file: the
                     * buffer is in memory, so :w is the
                     * whole of what preserving means.
                     */
                    tail2of("preserve");
                    eol();
                    if (ex_thrown)
                        continue;
                    THROWC(error("Preserve is not supported - use :w"));
                }
                tail2of("print");
                break;

            default:
                tail("print");
                break;
            }
            /* print */
            setCNL();
            if (ex_thrown)
                continue;
            pflag = 0;
        print:
            nonzero();
            if (ex_thrown)
                continue;
            if (CL && span() > LINES) {
                flush1();
                vclear();
            }
            co_await plines(addr1, addr2, 1);
            if (out_pending())
                co_await exflush();
            continue;

            /* quit */
        case 'q':
            tail("quit");
            setnoaddr();
            if (ex_thrown)
                continue;
            c = quickly();
            eol();
            if (ex_thrown)
                continue;
            if (!c)
                nomore();
        quit:
            if (inopen) {
                vgoto(WECHO, 0);
                if (!ateopr())
                    vnfl();
                flush();
            }
            cleanup(1);
            ex_exit(0);
            co_return (0);

        case 'r':
            if (peekchar() == 'e') {
                ignchar();
                switch (peekchar()) {
                    /* rewind */
                case 'w':
                    tail2of("rewind");
                    setnoaddr();
                    if (ex_thrown)
                        continue;
                    if (!exclam()) {
                        ckaw();
                        if (chng && dol > zero)
                            THROWC(error("No write@since last chage (:rewind! overrides)"));
                    }
                    eol();
                    if (ex_thrown)
                        continue;
                    erewind();
                    co_await next();
                    c = 'e';
                    ungetchar(lastchar());
                    filename(c);
                    goto doecmd;

                    /* recover */
                case 'c':
                    tail2of("recover");
                    setnoaddr();
                    if (ex_thrown)
                        continue;
                    c = 'e';
                    if (!exclam() && chng)
                        c = 'E';
                    filename(c);
                    if (c == 'E') {
                        ungetchar(lastchar());
                        ignore(quickly());
                    }
                    THROWC(error("Recover is not supported"));
                    nochng();
                    continue;
                }
                tail2of("read");
            } else
                tail("read");
            /* read */
            if (savedfile[0] == 0 && dol == zero)
                c = 'e';
            pastwh();
            co_await vmacchng(0);
            if (peekchar() == '!') {
                setdot();
                if (ex_thrown)
                    continue;
                ignchar();
                co_await unix0(0);
                co_await filter(0);
                continue;
            }
            filename(c);
            co_await rop(c);
            nochng();
            if (inopen && endline && addr1 > zero && addr1 < dol)
                dot = addr1 + 1;
            continue;

        case 's':
            switch (peekchar()) {
                /*
                 * Caution: 2nd char cannot be c, g, or r
                 * because these have meaning to substitute.
                 */

                /* set */
            case 'e':
                tail("set");
                setnoaddr();
                if (ex_thrown)
                    continue;
                set();
                continue;

                /* shell */
            case 'h':
                tail("shell");
                setNAEOL();
                if (ex_thrown)
                    continue;
                vnfl();
                flush();
                co_await vspawn_begin();
                co_await unixex((char *)"-i", (char *)"", 0, 0);
                co_await vspawn_end();
                co_await unixwt(1, 0);
                co_await vcontin(0);
                continue;

                /* source */
            case 'o':
#ifdef notdef
                if (inopen)
                    goto notinvis;
#endif
                tail("source");
                setnoaddr();
                if (ex_thrown)
                    continue;
                getone();
                if (ex_thrown)
                    continue;
                eol();
                if (ex_thrown)
                    continue;
                co_await source(file, 0);
                continue;
            }
            /* fall into ... */

            /* & */
            /* ~ */
            /* substitute */
        case '&':
        case '~':
            Command = "substitute";
            if (c == 's')
                tail(Command);
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            if (!co_await substitute(c))
                pflag = 0;
            continue;

            /* t */
        case 't':
            if (peekchar() == 'a') {
                tail("tag");
                co_await tagfind(exclam());
                if (!inopen)
                    lchng = chng - 1;
                else
                    nochng();
                continue;
            }
            tail("t");
            co_await vmacchng(0);
            co_await move();
            continue;

        case 'u':
            if (peekchar() == 'n') {
                ignchar();
                switch (peekchar()) {
                    /* unmap */
                case 'm':
                    tail2of("unmap");
                    setnoaddr();
                    if (ex_thrown)
                        continue;
                    mapcmd(1, 0);
                    continue;
                    /* unabbreviate */
                case 'a':
                    tail2of("unabbreviate");
                    setnoaddr();
                    if (ex_thrown)
                        continue;
                    mapcmd(1, 1);
                    anyabbrs = 1;
                    continue;
                }
                /* undo */
                tail2of("undo");
            } else
                tail("undo");
            setnoaddr();
            if (ex_thrown)
                continue;
            markDOT();
            c = exclam();
            newline();
            if (ex_thrown)
                continue;
            co_await undo(c);
            continue;

        case 'v':
            switch (peekchar()) {
            case 'e':
                /* version */
                tail("version");
                setNAEOL();
                if (ex_thrown)
                    continue;
                printf("@(#) Version 3.6, 11/3/80." + 5);
                noonl();
                continue;

                /* visual */
            case 'i':
                tail("visual");
                if (inopen) {
                    c = 'e';
                    goto editcmd;
                }
                co_await vop();
                pflag = 0;
                nochng();
                continue;
            }
            /* v */
            tail("v");
            co_await global(0);
            nochng();
            continue;

            /* write */
        case 'w':
            c = peekchar();
            tail((char *)(c == 'q' ? "wq" : "write"));
        wq:
            if (skipwh() && peekchar() == '!') {
                pofix();
                ignchar();
                setall();
                if (ex_thrown)
                    continue;
                co_await unix0(0);
                co_await filter(1);
            } else {
                setall();
                if (ex_thrown)
                    continue;
                co_await wop(1);
                nochng();
            }
            if (c == 'q')
                goto quit;
            continue;

            /* xit */
        case 'x':
            tail("xit");
            if (!chng)
                goto quit;
            c = 'q';
            goto wq;

            /* yank */
        case 'y':
            tail("yank");
            c = cmdreg();
            setcount();
            if (ex_thrown)
                continue;
            eol();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            if (c)
                YANKreg(c);
            else
                yank();
            continue;

            /* z */
        case 'z':
            co_await zop(0);
            pflag = 0;
            continue;

            /* * */
            /* @ */
        case '*':
        case '@':
            c = getchar();
            if (c == '\n' || c == '\r')
                ungetchar(c);
            if (any(c, "@*\n\r"))
                c = lastmac;
            if (isupper(c))
                c = tolower(c);
            if (!islower(c))
                THROWC(error("Bad register"));
            newline();
            if (ex_thrown)
                continue;
            setdot();
            if (ex_thrown)
                continue;
            co_await cmdmac(c);
            continue;

            /* | */
        case '|':
            endline = 0;
            goto caseline;

            /* \n */
        case '\n':
            endline = 1;
        caseline:
            notempty();
            if (ex_thrown)
                continue;
            if (addr2 == 0) {
                if (UP != NOSTR && c == '\n' && !inglobal)
                    c = CTRL('k');
                if (inglobal)
                    addr1 = addr2 = dot;
                else {
                    if (dot == dol)
                        THROWC(error("At EOF|At end-of-file"));
                    addr1 = addr2 = dot + 1;
                }
            }
            setdot();
            if (ex_thrown)
                continue;
            nonzero();
            if (ex_thrown)
                continue;
            if (seensemi)
                addr1 = addr2;
            getline(*addr1);
            if (c == CTRL('k')) {
                flush1();
                destline--;
                if (hadpr)
                    shudclob = 1;
            }
            co_await plines(addr1, addr2, 1);
            if (out_pending())
                co_await exflush();
            continue;

            /* " */
        case '"':
            comment();
            continue;

            /* # */
        case '#':
        numberit:
            setCNL();
            if (ex_thrown)
                continue;
            ignorf(setnumb(1));
            pflag = 0;
            goto print;

            /* = */
        case '=':
            newline();
            if (ex_thrown)
                continue;
            setall();
            if (ex_thrown)
                continue;
            if (inglobal == 2)
                pofix();
            printf("%d", lineno(addr2));
            noonl();
            continue;

            /* ! */
        case '!':
            if (addr2 != 0) {
                co_await vmacchng(0);
                co_await unix0(0);
                setdot();
                if (ex_thrown)
                    continue;
                co_await filter(2);
            } else {
                co_await unix0(1);
                COCHKV(0);
                pofix();
                flush();
                co_await vspawn_begin();
                co_await unixex((char *)"-c", uxb, 0, 0);
                co_await vspawn_end();
                co_await unixwt(1, 0);
                co_await vcontin(0);
                nochng();
            }
            continue;

            /* < */
            /* > */
        case '<':
        case '>':
            for (cnt = 1; peekchar() == c; cnt++)
                ignchar();
            setCNL();
            if (ex_thrown)
                continue;
            co_await vmacchng(0);
            shift(c, cnt);
            continue;

            /* ^D */
            /* EOF */
        case CTRL('d'):
        case EOF:
            if (exitoneof) {
                if (addr2 != 0)
                    dot = addr2;
                co_return (0);
            }
            if (!intty)
                co_return (0);
            if (addr2 != 0) {
                setlastchar('\n');
                putnl();
            }
            if (dol == zero) {
                if (addr2 == 0)
                    putnl();
                notempty();
                if (ex_thrown)
                    continue;
            }
            ungetchar(EOF);
            co_await zop(hadpr);
            continue;

        default:
            if (!isalpha(c))
                break;
            ungetchar(c);
            tailprim("", 0, 0);
        }
        THROWC(error("What?|Unknown command character '%c'", c));
    }
}
