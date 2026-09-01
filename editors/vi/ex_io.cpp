/* Copyright (c) 1980 Regents of the University of California */
#include "ex.h"
#include "ex_buf.h"

#include "kernel/alloc.h"
#include "ex_argv.h"
#include "ex_buf.h"
#include "ex_screen.h"
#include "ex_vis.h"

/*
 * File input/output, source, preserve and recover
 */

/*
 * Following remember where . was in the previous file for return
 * on file switching.
 */
int altdot;
int oldadot;
exbool wasalt;
short isalt;

long cntch;   /* Count of characters on unit io */
short cntln;  /* Count of lines " */
long cntnull; /* Count of nulls " */
long cntodd;  /* Count of malformed UTF-8 sequences " */

/*
 * Parse file name for command encoded by comm.
 * If comm is E then command is doomed and we are
 * parsing just so user won't have to retype the name.
 */
void filename(int comm)
{
    int c = comm, d;
    int i;

    d = getchar();
    if (endcmd(d)) {
        if (savedfile[0] == 0 && comm != 'f')
            THROW(error("No file|No current filename"));
        CP(file, savedfile);
        wasalt  = (isalt > 0) ? isalt - 1 : 0;
        isalt   = 0;
        oldadot = altdot;
        if (c == 'e' || c == 'E')
            altdot = lineDOT();
        if (d == EOF)
            ungetchar(d);
    } else {
        ungetchar(d);
        getone();
        eol();
        if (savedfile[0] == 0 && c != 'E' && c != 'e') {
            c      = 'e';
            edited = 0;
        }
        wasalt  = strcmp(file, altfile) == 0;
        oldadot = altdot;
        switch (c) {
        case 'f':
            edited = 0;
            /* fall into ... */

        case 'e':
            if (savedfile[0]) {
                altdot = lineDOT();
                CP(altfile, savedfile);
            }
            CP(savedfile, file);
            break;

        default:
            if (file[0]) {
                if (c != 'E')
                    altdot = lineDOT();
                CP(altfile, file);
            }
            break;
        }
    }
    if (hush && comm != 'f' || comm == 'E')
        return;
    if (file[0] != 0) {
        lprintf("\"%s\"", file);
        if (comm == 'f') {
            if (value(READONLY))
                printf(" [Read only]");
            if (!edited)
                printf(" [Not edited]");
            if (tchng)
                printf(" [Modified]");
        }
        flush();
    } else
        printf("No file ");
    if (comm == 'f') {
        if (!(i = lineDOL()))
            i++;
        printf(" line %d of %d --%ld%%--", lineDOT(), lineDOL(), (long)100 * lineDOT() / i);
    }
}

/*
 * Get the argument words for a command into genbuf
 * expanding # and %.
 */
int getargs(void)
{
    int c;
    char *cp, *fp;
    static char fpatbuf[32]; /* hence limit on :next +/pat */

    pastwh();
    if (peekchar() == '+') {
        for (cp = fpatbuf;;) {
            c = *cp++ = getchar();
            if (cp >= &fpatbuf[sizeof(fpatbuf)])
                THROWV(0, error("Pattern too long"));
            if (c == '\\' && isspace(peekchar()))
                c = getchar();
            if (c == EOF || isspace(c)) {
                ungetchar(c);
                *--cp    = 0;
                firstpat = &fpatbuf[1];
                break;
            }
        }
    }
    if (skipend())
        return (0);
    CP(genbuf, "echo ");
    cp = &genbuf[5];
    for (;;) {
        c = getchar();
        if (endcmd(c)) {
            ungetchar(c);
            break;
        }
        switch (c) {
        case '\\':
            if (any(peekchar(), "#%|"))
                c = getchar();
            /* fall into... */

        default:
            if (cp > &genbuf[LBSIZE - 2])
            flong:
                THROWV(0, error("Argument buffer overflow"));
            *cp++ = c;
            break;

        case '#':
            fp = altfile;
            if (*fp == 0)
                THROWV(0, error("No alternate filename@to substitute for #"));
            goto filexp;

        case '%':
            fp = savedfile;
            if (*fp == 0)
                THROWV(0, error("No current filename@to substitute for %%"));
        filexp:
            while (*fp) {
                if (cp > &genbuf[LBSIZE - 2])
                    goto flong;
                *cp++ = *fp++;
            }
            break;
        }
    }
    *cp = 0;
    return (1);
}

/*
 * Glob the argument words in genbuf, or if no globbing
 * is implied, just split them up directly.
 */
/*
 * Split the argument words in genbuf.
 *
 * Upstream globbed them, by writing "echo <words>" down a pipe to a forked
 * shell and reading the expansion back -- the shell was the only thing that
 * knew what * and ? and ~ meant. Here the shell has already done that before
 * ex was entered, so a name that reaches this has been expanded already, and
 * what is left is the splitting. See the note in the README: :e *.c is the
 * shell's business, not the editor's.
 */
void glob(struct glob *gp)
{
    char **argv = gp->argv;
    char *cp    = gp->argspac;
    char *v     = genbuf + 5; /* strlen("echo ") */

    gp->argc0 = 0;
    for (;;) {
        while (isspace(*v))
            v++;
        if (!*v)
            break;
        *argv++ = cp;
        while (*v && !isspace(*v))
            *cp++ = *v++;
        *cp++ = 0;
        gp->argc0++;
        if (gp->argc0 >= NARGS)
            THROW(error("Arg list too long"));
    }
    *argv = 0;
}

/*
 * Scan genbuf for shell metacharacters.
 * Set is union of v7 shell and csh metas.
 */
int gscan(void)
{
    char *cp;

    for (cp = genbuf; *cp; cp++)
        if (any(*cp, "~{[*?$`'\"\\"))
            return (1);
    return (0);
}

/*
 * Parse one filename into file.
 */
void getone(void)
{
    char *str;
    struct glob G;

    if (getargs() == 0)
        THROW(error("Missing filename"));
    glob(&G);
    CHK;
    if (G.argc0 > 1)
        THROW(error("Ambiguous|Too many file names"));
    str = G.argv[G.argc0 - 1];
    if (strlen(str) > FNSIZE - 4)
        THROW(error("Filename too long"));
    CP(file, str);
}

/*
 * Read a file from the world.
 * C is command, 'e' if this really an edit (or a recover).
 */
Task<void> rop(int c)
{
    static int ovro;   /* old value(READONLY) */
    static int denied; /* 1 if READONLY was set due to file permissions */
    struct stat stbuf;

    io = co_await b_open(file, O_RDONLY);
    if (io < 0) {
        if (c == 'e' && errno == ENOENT) {
            edited++;
            /*
             * If the user just did "ex foo" he is probably
             * creating a new file.  Don't be an error, since
             * this is ugly, and it screws up the + option.
             */
            if (!seenprompt) {
                printf(" [New file]");
                noonl();
                co_return;
            }
        }
        COTHROW(syserror());
    }
    if (co_await b_fstat(io, &stbuf))
        COTHROW(syserror());
    if (S_ISDIR(stbuf.st_mode))
        COTHROW(error(" Directory"));
    if (c != 'r') {
        if (value(READONLY) && denied) {
            value(READONLY) = ovro;
            denied          = 0;
        }
    }
    if (value(READONLY)) {
        printf(" [Read only]");
        flush();
    }
    if (c == 'r')
        setdot();
    else
        setall();
    if (FIXUNDO && inopen && c == 'r')
        undap1 = undap2 = dot + 1;
    co_await rop2();
    COCHK;
    co_await rop3(c);
}

Task<void> rop2(void)
{
    deletenone();
    clrstats();
    ignore(co_await append(getfile, addr2));
}

Task<void> rop3(int c)
{
    if (co_await iostats() == 0 && c == 'e')
        edited++;
    if (c == 'e') {
        if (wasalt || firstpat) {
            line *addr = zero + oldadot;

            if (addr > dol)
                addr = dol;
            if (firstpat) {
                globp = (*firstpat) ? firstpat : (char *)"$";
                co_await commands(1, 1);
                firstpat = 0;
            } else if (addr >= one) {
                if (inopen)
                    dot = addr;
                markpr(addr);
            } else
                goto other;
        } else
        other:
            if (dol > zero) {
                if (inopen)
                    dot = one;
                markpr(one);
            }
        if (FIXUNDO)
            undkind = UNDNONE;
        if (inopen) {
            vcline = 0;
            vreplace(0, LINES, lineDOL());
        }
    }
    if (laste) {
        laste = 0;
        sync();
    }
}

/*
 * Are these two really the same inode?
 */
exbool samei(struct stat *sp, char *cp)
{
    (void)sp;
    return (eq(file, cp));
}

/* Returns from edited() */
#define EDF     0  /* Edited file */
#define NOTEDF  -1 /* Not edited file */
#define PARTBUF 1  /* Write of partial buffer to Edited file */

/*
 * Write a file.
 */
Task<void> wop(exbool dofname)
{
    int c, exclam, nonexist;
    line *saddr1, *saddr2;
    struct stat stbuf;

    c      = 0;
    exclam = 0;
    if (dofname) {
        if (peekchar() == '!')
            exclam++, ignchar();
        ignore(skipwh());
        while (peekchar() == '>')
            ignchar(), c++, ignore(skipwh());
        if (c != 0 && c != 2)
            COTHROW(error("Write forms are 'w' and 'w>>'"));
        filename('w');
        COCHK;
    } else {
        if (savedfile[0] == 0)
            COTHROW(error("No file|No current filename"));
        saddr1 = addr1;
        saddr2 = addr2;
        addr1  = one;
        addr2  = dol;
        CP(file, savedfile);
        if (inopen) {
            vclrech(0);
            splitw++;
        }
        lprintf("\"%s\"", file);
    }
    nonexist = co_await b_stat(file, &stbuf);
    switch (c) {
    case 0:
        if (!exclam && (!value(WRITEANY) || value(READONLY)))
            switch (edfile()) {
            case NOTEDF:
                if (nonexist)
                    break;
                COTHROW(serror(" File exists| File exists - use \"w! %s\" to overwrite", file));

            case EDF:
                if (value(READONLY))
                    COTHROW(error(" File is read only"));
                break;

            case PARTBUF:
                if (value(READONLY))
                    COTHROW(error(" File is read only"));
                COTHROW(error(" Use \"w!\" to write partial buffer"));
            }
    cre:
        io = co_await b_creat(file, 0644);
        if (io < 0)
            COTHROW(syserror());
        writing = 1;
        if (hush == 0)
            if (nonexist)
                printf(" [New file]");
            else if (value(WRITEANY) && edfile() != EDF)
                printf(" [Existing file]");
        break;

    case 2:
        io = co_await b_open(file, O_WRONLY);
        if (io < 0) {
            if (exclam || value(WRITEANY))
                goto cre;
            COTHROW(syserror());
        }
        co_await b_lseek(io, 0, SEEK_END);
        break;
    }
    co_await putfile();
    COCHK;
    ignore(co_await iostats());
    if (c != 2 && addr1 == one && addr2 == dol) {
        if (eq(file, savedfile))
            edited = 1;
        sync();
    }
    if (!dofname) {
        addr1 = saddr1;
        addr2 = saddr2;
    }
    writing = 0;
}

/*
 * Is file the edited file?
 * Work here is that it is not considered edited
 * if this is a partial buffer, and distinguish
 * all cases.
 */
exbool edfile(void)
{
    if (!edited || !eq(file, savedfile))
        return (NOTEDF);
    return (addr1 == one && addr2 == dol ? EDF : PARTBUF);
}

/*
 * Extract the next line from the io stream.
 */
static char *nextip;
static int ninbuf;

Task<int> getfile(void)
{
    short c;
    char *lp, *fp;

    lp = linebuf;
    fp = nextip;
    do {
        if (--ninbuf < 0) {
            ninbuf = co_await b_read(io, genbuf, LBSIZE) - 1;
            if (ninbuf < 0) {
                if (lp != linebuf) {
                    lp++;
                    printf(" [Incomplete last line]");
                    break;
                }
                co_return (EOF);
            }
            fp = genbuf;
            cntch += ninbuf + 1;
        }
        if (lp >= &linebuf[LBSIZE]) {
            COTHROWV(0, error(" Line too long"));
        }
        c = (unsigned char)*fp++;
        if (c == 0) {
            cntnull++;
            continue;
        }
        *lp++ = c;
    } while (c != '\n');
    *--lp = 0;
    /* Not "non-ASCII" any more: a line is UTF-8, and this is what is not. */
    for (char *p = linebuf; *p;) {
        int n, r = runeat(p, &n);

        if (r == 0xfffd)
            cntodd++;
        p += n;
    }
    nextip = fp;
    cntln++;
    co_return (0);
}

/*
 * Write a range onto the io stream.
 */
Task<void> putfile(void)
{
    line *a1;
    char *fp, *lp;
    int nib;

    a1 = addr1;
    clrstats();
    cntln = addr2 - a1 + 1;
    if (cntln == 0)
        co_return;
    nib = BUFSIZ;
    fp  = genbuf;
    do {
        getline(*a1++);
        lp = linebuf;
        for (;;) {
            if (--nib < 0) {
                nib = fp - genbuf;
                if (co_await b_write(io, genbuf, nib) != nib) {
                    wrerror();
                    COCHK;
                }
                cntch += nib;
                nib = BUFSIZ - 1;
                fp  = genbuf;
            }
            if ((*fp++ = *lp++) == 0) {
                fp[-1] = '\n';
                break;
            }
        }
    } while (a1 <= addr2);
    nib = fp - genbuf;
    if (co_await b_write(io, genbuf, nib) != nib) {
        wrerror();
        COCHK;
    }
    cntch += nib;
}

/*
 * A write error has occurred;  if the file being written was
 * the edited file then we consider it to have changed since it is
 * now likely scrambled.
 */
void wrerror(void)
{
    if (eq(file, savedfile) && edited)
        change();
    THROW(syserror());
}

/*
 * Source command, handles nested sources.
 * Traps errors since it mungs unit 0 during the source.
 */
short slevel;
short ttyindes;

Task<void> source(char *fil, exbool okfail)
{
    int ointty;
    char savepeekc, *saveglobp;
    char *text;

    savepeekc = peekc;
    saveglobp = globp;
    peekc     = 0;
    globp     = 0;

    {
        Result<String> r = Err(Error::NoMemory);

        if (Task<Result<String>> t = read_file(Str(fil, strlen(fil))))
            r = co_await t;
        if (r.is_err()) {
            errno = errno_of(r.error());
            peekc = savepeekc;
            globp = saveglobp;
            if (!okfail)
                COTHROW(filioerr(fil));
            co_return;
        }
        {
            Str got = res_of(r).str();

            text = (char *)heap_alloc(got.size() + 1);
            if (text == 0) {
                peekc = savepeekc;
                globp = saveglobp;
                COTHROW(error(" Out of memory"));
            }
            memcpy(text, got.data(), got.size());
            text[got.size()] = 0;
        }
    }

    slevel++;
    ointty        = intty;
    intty         = 0;
    oprompt       = value(PROMPT);
    value(PROMPT) = 0;
    globp         = text;
    co_await commands(1, 1);
    intty         = ointty;
    value(PROMPT) = oprompt;
    globp         = saveglobp;
    peekc         = savepeekc;
    slevel--;
    heap_free(text);
}

/*
 * Clear io statistics before a read or write.
 */
void clrstats(void)
{
    ninbuf  = 0;
    cntch   = 0;
    cntln   = 0;
    cntnull = 0;
    cntodd  = 0;
}

/*
 * Io is finished, close the unit and print statistics.
 */
Task<int> iostats(void)
{
    co_await b_close(io);
    io = -1;
    if (hush == 0) {
        if (value(TERSE))
            printf(" %d/%ld", cntln, cntch);
        else
            printf(" %d line%s, %ld character%s", cntln, plural((long)cntln), cntch, plural(cntch));
        if (cntnull || cntodd) {
            printf(" (");
            if (cntnull) {
                printf("%ld null", cntnull);
                if (cntodd)
                    printf(", ");
            }
            if (cntodd)
                printf("%ld malformed", cntodd);
            putchar(')');
        }
        noonl();
        flush();
    }
    co_return (cntnull != 0 || cntodd != 0);
}
