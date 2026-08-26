// ttyio.cpp — ttyio.c, by Info-ZIP. Reading a password without echoing it.
//
// Upstream opened /dev/tty, turned off ECHO with termios, and read a byte at a
// time. There is no tty device and no termios here: the console is claimed
// with keys_claim(), which is what stops the shell's editor seeing the keys,
// and every key arrives through key_read() unechoed already — so there is no
// echo to turn off and echoff()/echon() have nothing to do.

#include "ttyio.h"

#include "proc/rt.h"

// A password, read without echoing it. Answers p, or NULL if this is not a
// terminal — upstream's "stderr is not a tty".
Task<char *> getp(ZCONST char *m, char *p, int n)
{
    int c; // one key
    int i; // number of characters input
    ZCONST char *w;

    Result<TtyInfo> t = Err(Error::NoMemory);
    if (Task<Result<TtyInfo>> tk = tty_of(SYS_STDIN))
        t = co_await tk;
    if (t.is_err() || !t.value().console)
        co_return NULL;

    if (Task<Result<Geometry>> tk = keys_claim(true))
        co_await tk;

    // get password
    w = "";
    do {
        co_await zfputs(w, zstderr); // warning if back again
        co_await zfputs(m, zstderr); // display prompt and flush
        co_await zfflush(zstderr);
        i = 0;
        do { // read line, keeping first n characters
            Result<KeyPress> k = Err(Error::NoMemory);
            if (Task<Result<KeyPress>> tk = key_read())
                k = co_await tk;
            if (k.is_err()) {
                if (Task<Result<Geometry>> g = keys_claim(false))
                    co_await g;
                co_return NULL;
            }
            c = (int)k.value().code;
            if (c == '\r')
                c = '\n'; // until user hits CR
            if (c == 8 || c == 127) {
                if (i > 0)
                    i--; // the `backspace' and `del' keys works
            } else if (i < n)
                p[i++] = (char)c; // truncate past n
        } while (c != '\n');
        co_await zfputc('\n', zstderr);
        co_await zfflush(zstderr);
        w = "(line too long--try again)\n";
    } while (p[i - 1] != '\n');
    p[i - 1] = 0; // terminate at newline

    if (Task<Result<Geometry>> g = keys_claim(false))
        co_await g;

    co_return p; // return pointer to password
}
