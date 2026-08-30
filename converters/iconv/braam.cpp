// What the port kit has not got: the wide half, the locale stubs, and citrus's
// own errno-to-Error mapping. The C library proper is the kit's, asked for with
// PORT in CMakeLists.txt.

#include "braam.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "kernel/alloc.h"
#include "kernel/host.h"
#include "kernel/text.h"
#include "proc/io.h"

// ---------------------------------------------------------------- wide chars
//
// The locale is UTF-8 and wchar_t is UTF-32, so the Apple wchar_t extension's
// two conversions are the kernel's own codec. A sequence that straddles a call
// is held in the state, which is what makes these restartable.

extern "C" usize mbrtowc(wchar_t *pwc, const char *s, usize n, mbstate_t *ps)
{
    static mbstate_t own;
    if (!ps)
        ps = &own;

    if (!s) { // reset
        ps->len = 0;
        return 0;
    }

    usize used = 0;
    while (used < n) {
        if (ps->len >= sizeof(ps->buf))
            return (usize)-1;
        ps->buf[ps->len++] = (unsigned char)s[used++];

        char32_t ch  = 0;
        usize wanted = utf8_decode(Str((const char *)ps->buf, ps->len), 0, ch);

        // Nothing consumed means the sequence runs past what is held; ask for
        // another byte rather than deciding.
        if (wanted == 0)
            continue;
        if (wanted != ps->len)
            continue;

        ps->len = 0;
        if (pwc)
            *pwc = (wchar_t)ch;
        return ch == 0 ? 0 : used;
    }
    return (usize)-2; // incomplete, and every byte was taken
}

extern "C" usize wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
    if (ps)
        ps->len = 0;
    if (!s)
        return 1; // the reset sequence, which UTF-8 does not have
    return utf8_encode((char32_t)wc, s);
}

// ------------------------------------------------------------------ the stubs

// There is one locale and it is UTF-8. Reached only for the empty, "char" and
// "wchar_t" encoding names.
extern "C" char *nl_langinfo(int)
{
    return (char *)"UTF-8";
}
extern "C" char *locale_charset(void)
{
    return (char *)"UTF-8";
}

// No setuid here, so the environment is always trusted.
extern "C" int issetugid(void)
{
    return 0;
}

extern "C" void iconv_assert_fail(const char *what, const char *file, int line)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "iconv: assertion failed: %s at %s:%d", what, file, line);
    panic(Str(buf, n < 0 ? 0 : (usize)n));
}

extern "C" void abort(void)
{
    panic("iconv: internal error");
}

// Nothing in the library exits; the command returns a status up its call chain.
extern "C" void exit(int)
{
    panic("iconv: exit from the library");
}

// Not error_of() from compat/cerr.h, though it has the same shape: citrus
// means E2BIG as "the output buffer filled" and EILSEQ as "bad input", which
// the kit's bridge folds onto Error::Io. This is iconv's policy.
Error iconv_error(int err)
{
    switch (err) {
    case 0:
        return Error::Invalid;
    case ENOENT:
        return Error::NotFound;
    case ENOMEM:
        return Error::NoMemory;
    case EILSEQ:
    case EINVAL:
    case EFTYPE:
        return Error::Invalid;
    case E2BIG:
        return Error::Again;
    case EOPNOTSUPP:
        return Error::Unsupported;
    default:
        return Error::Io;
    }
}
