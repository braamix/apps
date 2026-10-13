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

// fork() and exec() are one spawn(): the child is entered with this process's
// three streams and nothing else, so there is nothing for v7's doclose() to do.
static Task<int> run(Span<const Str> argv)
{
    Result<u32> r = co_await spawn(Args{ argv });

    if (r.is_err()) {
        if (r.error() == Error::NotFound)
            diag("Make: Cannot load %.*s.  Stop.\n", (int)argv[0].size(), argv[0].data());
        else
            diag("Make: %.*s: %.*s.  Stop.\n", (int)argv[0].size(), argv[0].data(),
                 (int)error_name(r.error()).size(), error_name(r.error()).data());
        co_return 1 << 8;
    }
    childpid = r.value();
    co_return co_await await();
}

static Task<int> doshell(char *comstring, int nohalt)
{
    // This sh takes its flags one word each: -ce would be -c and a command "e".
    Str cmd   = Str(comstring, strlen(comstring));
    Str ce[4] = { SHELLCOM, "-e", "-c", cmd };
    Str c[3]  = { SHELLCOM, "-c", cmd };

    if (nohalt)
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
    // At file scope, not in a frame: 200 words are past a coroutine's 512
    // bytes, and only one command runs at a time.
    static Str argv[200];
    Str *p;

    while (*str == ' ' || *str == '\t')
        ++str;
    if (*str == '\0')
        co_return -1; // no command

    p = argv;
    for (t = str; *t;) {
        if (p >= &argv[sizeof(argv) / sizeof(argv[0]) - 1]) {
            diag("Too many arguments\n");
            co_return -1;
        }
        char *w = t;
        while (*t != ' ' && *t != '\t' && *t != '\0')
            ++t;
        *p++ = Str(w, t - w);
        if (*t)
            for (*t++ = '\0'; *t == ' ' || *t == '\t'; ++t)
                ;
    }

    co_return co_await run(Span<const Str>(argv, p - argv));
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
