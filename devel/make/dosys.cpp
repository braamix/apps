/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "proc/rt.h"

static int metas(char *s);
static Task<int> doshell(char *comstring, int nohalt);
static Task<int> doexec(char *str);
static Task<int> await(void);

Task<int> dosys(char *comstring, int nohalt)
{
    int status;

    // The command line has been said; what the command says comes after it.
    co_await flushout();
    if (metas(comstring))
        status = co_await doshell(comstring, nohalt);
    else
        status = co_await doexec(comstring);

    co_return status;
}

// Are there any Shell meta-characters?  funny['\0'] carries META, so the NUL
// ends the loop and its own value is the answer.
static int metas(char *s)
{
    char c;

    while ((funny[(unsigned char)(c = *s++)] & META) == 0)
        ;
    return c;
}

// A child's environment, made for each child: this process's, with every
// exported macro at its value now and the MAKEFLAGS make_main() made, and the
// unexported ones out.  A macro still as the environment had it is passed as
// it was, not expanded.  GNU's.
static char *cmakeflags;
static Str *cenv;
static char **cdefs;
static int ncdefs;
static Args cenvargs;

void childenv(const char *makeflags)
{
    free(cmakeflags);
    cmakeflags = (char *)ckalloc(strlen(makeflags) + sizeof("MAKEFLAGS="));
    strcpy(cmakeflags, "MAKEFLAGS=");
    strcat(cmakeflags, makeflags);
}

// Whether a child sees v as a macro of this make's rather than as it came.
static int expvar(struct varblock *vp)
{
    const char *s;

    if (vp->varval == 0 || vp->exported < 0 || vp->fromenv || !unequal(vp->varname, "MAKEFLAGS"))
        return NO;
    if (vp->exported == 0 && !exportall)
        return NO;
    for (s = vp->varname; *s; ++s) // a name the shell can take
        if (!(*s == '_' || (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
              (s > vp->varname && *s >= '0' && *s <= '9')))
            return NO;
    return s > vp->varname;
}

Args *mkenv(void)
{
    usize i, n = proc_env_count();
    struct varblock *vp;
    char *name, *val;
    int nv, k, j;
    Str e;

    if (cmakeflags == NULL)
        return nullptr;
    for (j = 0; j < ncdefs; ++j)
        free(cdefs[j]);
    free(cdefs);
    free(cenv);
    for (nv = 0, vp = firstvar; vp; vp = vp->nxtvarblock)
        nv += expvar(vp);
    cenv   = (Str *)ckalloc((n + nv + 1) * sizeof(Str));
    cdefs  = (char **)ckalloc((nv + 1) * sizeof(char *));
    ncdefs = 0;
    for (i = 0, k = 0; i < n; ++i) {
        e = proc_env_at(i);
        if (e.starts_with("MAKEFLAGS="))
            continue;
        for (j = 0; j < (int)e.size() && e.data()[j] != '='; ++j)
            ;
        name = (char *)ckalloc(j + 1);
        memcpy(name, e.data(), j);
        vp = findvar(name);
        free(name);
        if (vp && vp->varval && (vp->exported < 0 || expvar(vp)))
            continue;
        cenv[k++] = e;
    }
    for (vp = firstvar; vp; vp = vp->nxtvarblock)
        if (expvar(vp)) {
            val  = vp->simple ? copys(vp->varval) : substa(vp->varval);
            name = (char *)ckalloc(strlen(vp->varname) + strlen(val) + 2);
            concat(vp->varname, "=", name);
            strcat(name, val);
            free(val);
            cdefs[ncdefs++] = name;
            cenv[k++]       = Str(name, strlen(name));
        }
    cenv[k++] = Str(cmakeflags, strlen(cmakeflags));
    cenvargs  = Args{ Span<const Str>(cenv, k) };
    return &cenvargs;
}

// Where run() puts the child it does not wait for, under -j.
static u32 *spawnpid;

// fork() and exec() are one spawn(): the child is entered with this process's
// three streams and nothing else, so there is nothing for v7's doclose() to do.
static Task<int> run(Span<const Str> argv)
{
    Args *env;
    Result<u32> r = Err(Error::Io);

    while ((env = mkenv()), substneed)
        co_await perform();
    r = co_await spawn(Args{ argv }, ChildIo{}, env);

    if (r.is_err()) {
        if (r.error() == Error::NotFound)
            diag("Make: Cannot load %.*s.  Stop.\n", (int)argv[0].size(), argv[0].data());
        else
            diag("Make: %.*s: %.*s.  Stop.\n", (int)argv[0].size(), argv[0].data(),
                 (int)error_name(r.error()).size(), error_name(r.error()).data());
        co_return 1 << 8;
    }
    if (spawnpid) { // a job's: reaped by whoever waits for any
        *spawnpid = r.value();
        co_return 0;
    }
    childpid = r.value();
    co_return co_await await();
}

// A command started and not waited for, under -j: 0 and *pidp, or the status
// it failed with.
Task<int> startsys(char *comstring, int nohalt, u32 *pidp)
{
    int status;

    *pidp    = 0;
    spawnpid = pidp;
    status   = co_await dosys(comstring, nohalt);
    spawnpid = NULL;
    co_return status;
}

// Set for a .ONESHELL script, which the shell runs as GNU's does, with -c
// alone: a script stops where it says, not at its first failure.
int shellonly = NO;

static Task<int> doshell(char *comstring, int nohalt)
{
    // This sh takes its flags one word each: -ce would be -c and a command "e".
    Str cmd   = Str(comstring, strlen(comstring));
    Str ce[4] = { SHELLCOM, "-e", "-c", cmd };
    Str c[3]  = { SHELLCOM, "-c", cmd };

    if (nohalt || shellonly)
        co_return co_await run(Span<const Str>(c, 3));
    co_return co_await run(Span<const Str>(ce, 4));
}

// The status, as wait(2) put it: the exit code in the second byte, which is what
// docom1() reads.  A ^C while waiting is passed to the child, and once it is gone
// make goes the way v7's intrupt() did.
static Task<int> await(void)
{
    Result<Exited> r = Err(Error::Io);
    int intr         = 0;

    for (;;) {
        r = co_await wait_child(childpid);
        if (r.is_ok())
            break;
        if (r.error() == Error::Intr && sig_take(SIG_INT)) {
            co_await kill_child(childpid, intr++ ? SIG_KILL : SIG_INT);
            continue;
        }
        if (r.error() == Error::Cancelled) {
            stop(130);
            co_return 1 << 8;
        }
        fatal("bad wait code");
        co_return 1 << 8;
    }
    childpid = 0;
    if (intr) {
        co_await intrupt();
        co_return 1 << 8;
    }
    co_return (r.value().status & 0xff) << 8;
}

static Task<int> doexec(char *str)
{
    char *t;
    Str *argv, *p;
    int n, status;

    while (*str == ' ' || *str == '\t')
        ++str;
    if (*str == '\0')
        co_return -1; // no command

    // As many words as there are: v7's argv was 200.
    for (n = 1, t = str; *t; ++t)
        if (*t == ' ' || *t == '\t')
            ++n;
    p = argv = (Str *)ckalloc(n * sizeof(Str));
    for (t = str; *t;) {
        char *w = t;
        while (*t != ' ' && *t != '\t' && *t != '\0')
            ++t;
        *p++ = Str(w, t - w);
        if (*t)
            for (*t++ = '\0'; *t == ' ' || *t == '\t'; ++t)
                ;
    }

    status = co_await run(Span<const Str>(argv, p - argv));
    free(argv);
    co_return status;
}

// What cmd wrote, run by the shell: its newlines spaces and a last one gone.
// Its status is not looked at, as POSIX has it for != and GNU for $(shell).
Task<char *> shellout(char *cmd)
{
    char *out = NULL, *p;
    size_t n           = 0;
    Args *env;
    Result<Piped> pipe = Err(Error::Io);
    Result<u32> r      = Err(Error::Io);

    co_await flushout();
    // Not waited for: an exported macro may name the very $(shell) being run.
    env       = mkenv();
    substneed = NO;
    pipe      = co_await make_pipe();
    if (pipe.is_err()) {
        fatal("Cannot make a pipe");
        co_return copys("");
    }
    {
        Str argv[3] = { SHELLCOM, "-c", Str(cmd, strlen(cmd)) };
        r           = co_await spawn(Args{ Span<const Str>(argv, 3) },
                                     ChildIo{ SYS_STDIN, (u32)pipe.value().w, SYS_STDERR }, env);
    }
    if (r.is_err()) {
        co_await close_fd(pipe.value().r);
        co_await close_fd(pipe.value().w);
        fatal("Couldn't load Shell");
        co_return copys("");
    }
    for (;;) {
        Result<String> c = co_await read_chunk(pipe.value().r);
        if (c.is_err())
            break;
        out = (char *)realloc(out, n + c.value().size() + 1);
        memcpy(out + n, c.value().data(), c.value().size());
        n += c.value().size();
    }
    co_await close_fd(pipe.value().r);
    co_await wait_child(r.value());
    if (out == NULL)
        out = copys("");
    out[n] = '\0';
    if (n > 0 && out[n - 1] == '\n')
        out[--n] = '\0';
    for (p = out; *p; ++p)
        if (*p == '\n')
            *p = ' ';
    co_return out;
}

// The `!=' eqsign() left: the command expanded and run by the shell, and what
// it wrote the macro's value.
Task<void> runset(void)
{
    char *cmd, *out, *name = shellname;
    int over = shellover;

    shellpend = NO;
    shellname = NULL;
    for (;;) {
        cmd = substnow(shellcmd);
        if (stopped || !substneed)
            break;
        free(cmd);
        co_await perform();
    }
    if (!stopped) {
        out = co_await shellout(cmd);
        forget(); // what the command did may change what the rest would say
        overriding = over;
        if (!setvar(name, out))
            free(out);
        overriding = NO;
    }
    free(cmd);
    free(name);
}

// v7 read a byte and wrote it back to move the time.  The store has a call for
// that, and creating a file is what is left of the rest.
Task<void> touch(int force, char *name)
{
    Result<FileInfo> st = co_await stat_of(Str(name, strlen(name)));
    FILE *f;

    if (st.is_err()) {
        if (force)
            goto create;
        else {
            diag("touch: file %s does not exist.\n", name);
            co_return;
        }
    }

    if (st.value().size == 0)
        goto create;

    if ((co_await touch_path(Str(name, strlen(name)))).is_err())
        goto bad;
    co_return;

bad:
    diag("Cannot touch %s\n", name);
    co_return;

create:
    if ((f = co_await b_fopen(name, "w")) == NULL)
        goto bad;
    co_await b_fclose(f);
}
