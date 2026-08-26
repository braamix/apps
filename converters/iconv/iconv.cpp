// iconv(1), from Citrus's iconv/iconv.c.
//
// The conversion loop is upstream's: read a block, hand it to __iconv until it
// is spent, write what came out, and move an incomplete trailing character to
// the front of the buffer before reading again. What changed is around it —
// there is no stdio, no getopt_long, and no exit() from inside a library.

#include "braam.h"
#include "iconv.h"
#include "iconv-internal.h"

#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/string.h"
#include "proc/file.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"

#include <errno.h>
#include <string.h>

namespace {

constexpr usize INBUFSIZE  = 1024;
constexpr usize OUTBUFSIZE = INBUFSIZE * 2;

// Three kilobytes of buffers would be three kilobytes of coroutine frame, and
// a frame past 512 bytes costs a whole 64 KiB span. They live in one heap
// block instead, taken once and freed at the end.
struct Bufs {
    char in[INBUFSIZE];
    char out[OUTBUFSIZE];
};

constexpr Str SHORT_USAGE =
    "Usage:\n"
    "    iconv [-cs] [-f <from-code>] [-t <to-code>] [file ...]\n"
    "    iconv -l\n"
    "Try `iconv --help' for the full list of options.\n";

constexpr Str HELP =
    "Usage:\n"
    "    iconv [-cs] [-f <from-code>] [-t <to-code>] [file ...]\n"
    "    iconv -l\n"
    "\n"
    "Convert text from one character encoding to another. Each file is read in\n"
    "turn, or standard input when none is named or the name is `-'; the result\n"
    "goes to standard output.\n"
    "\n"
    "Options:\n"
    "  -f <from-code>  the encoding the input is in         (default UTF-8)\n"
    "  -t <to-code>    the encoding to produce              (default UTF-8)\n"
    "  -c              drop characters the target cannot represent\n"
    "  -s              do not report how many were dropped\n"
    "  -l              list every encoding name the tables know\n"
    "  -h              print this message\n"
    "\n"
    "Long forms: --from-code=<name>, --to-code=<name>, --list, --silent, --help.\n"
    "\n"
    "A target name may carry //TRANSLIT to approximate what it cannot represent,\n"
    "or //IGNORE to drop it, which is what -c sets.\n"
    "\n"
    "Examples:\n"
    "    iconv -f KOI8-R -t UTF-8 old.txt >new.txt\n"
    "    iconv -c -f UTF-8 -t ASCII notes.txt\n"
    "    iconv -l\n";

Task<void> say(Str a, Str b = {}, Str c = {})
{
    Buf<256> m;
    m.put("iconv: ").put(a).put(b).put(c).put('\n');
    co_await write_all(SYS_STDERR, m.str());
}

// upstream's err(EXIT_FAILURE, ...), less the exit: the status travels up.
Task<void> fail(Str what, int err)
{
    Buf<256> m;
    m.put("iconv: ").put(what).put(": ").put(error_name(iconv_error(err))).put('\n');
    co_await write_all(SYS_STDERR, m.str());
}

// One stream, converted. Returns non-zero when the input held characters the
// conversion could not represent, which is upstream's exit status.
Task<int> do_conv(File &in, iconv_t cd, Bufs *b, bool silent, bool hide_invalid)
{
    File &out = File::stdout();

    // Not touched unless -c was given: the target may carry //IGNORE, and
    // clobbering that is worse than not setting this.
    if (hide_invalid) {
        int arg = 1;
        if (__bsd_iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, (void *)&arg) == -1) {
            co_await fail("iconvctl(DISCARD_ILSEQ)", errno);
            co_return -1;
        }
    }

    u64 invalids = 0;
    usize held   = 0; // an incomplete character carried from the last block

    for (;;) {
        Result<usize> got = co_await in.read(Span<char>(b->in + held, INBUFSIZE - held));
        if (got.is_err()) {
            if (got.error() == Error::Closed)
                break;
            co_return got.error() == Error::Cancelled ? -130 : -1;
        }
        usize inbytes = held + got.value();
        held          = 0;
        if (inbytes == 0)
            break;

        char *inp = b->in;
        while (inbytes > 0) {
            char *outp     = b->out;
            usize outbytes = OUTBUFSIZE;
            usize inval    = 0;

            usize ret = __bsd___iconv(cd, &inp, &inbytes, &outp, &outbytes, 0, &inval);
            invalids += inval;

            if (outbytes < OUTBUFSIZE)
                if ((co_await out.write(Str(b->out, OUTBUFSIZE - outbytes))).is_err())
                    co_return -1;

            if (ret != (usize)-1 || errno == E2BIG)
                continue;
            if (errno != EINVAL) {
                co_await fail("iconv()", errno);
                co_return -1;
            }

            // An incomplete character at the end of the block: keep it and
            // read the rest of it next time round.
            memmove(b->in, inp, inbytes);
            held    = inbytes;
            inbytes = 0;
        }
    }

    if (held > 0) {
        co_await say("unexpected end of file; the last character is incomplete.");
        co_return -1;
    }

    // The closing shift sequence, for a target that has one.
    char *outp     = b->out;
    usize outbytes = OUTBUFSIZE;
    if (__bsd_iconv(cd, nullptr, nullptr, &outp, &outbytes) == (usize)-1) {
        co_await fail("iconv()", errno);
        co_return -1;
    }
    if (outbytes < OUTBUFSIZE)
        if ((co_await out.write(Str(b->out, OUTBUFSIZE - outbytes))).is_err())
            co_return -1;

    if (invalids > 0 && !silent) {
        Buf<64> m;
        m.put("iconv: warning: invalid characters: ").put(invalids).put('\n');
        co_await write_all(SYS_STDERR, m.str());
    }
    co_return invalids > 0 ? 1 : 0;
}

// iconvlist walks the list with a plain callback, which cannot await, so the
// names are gathered here and written once afterwards. A namespace-scope
// global must be trivially destructible, hence the pointer.
String *LIST_OUT = nullptr;

int list_one(unsigned int n, const char *const *names, void *)
{
    for (unsigned int i = 0; i < n; i++) {
        if (!LIST_OUT->append(Str(names[i], strlen(names[i]))))
            return 0;
        if (!LIST_OUT->push(i < n - 1 ? ' ' : '\n'))
            return 0;
    }
    return 1;
}

} // namespace

Task<i32> proc_main(Args args)
{
    Str from, to;
    bool opt_c = false, opt_s = false, opt_l = false, opt_h = false;

    // OptParse has no long options, and scripts written against GNU iconv pass
    // them, so the four upstream declares are recognised ahead of it, plus
    // --help, which upstream does not have.
    Vec<Str> rest;
    bool only_operands = false;
    for (usize i = 1; i < args.size(); i++) {
        Str a = args[i];
        if (only_operands || a.size() < 2 || a[0] != '-') {
            if (!rest.push(a))
                co_return 1;
            continue;
        }
        if (a == "--") {
            only_operands = true;
            continue;
        }
        if (a == "--list") {
            opt_l = true;
        } else if (a == "--help") {
            opt_h = true;
        } else if (a == "--silent") {
            opt_s = true;
        } else if (a.starts_with("--from-code=")) {
            from = a.substr(12);
        } else if (a.starts_with("--to-code=")) {
            to = a.substr(10);
        } else if (a == "--from-code" || a == "--to-code") {
            if (i + 1 >= args.size()) {
                co_await write_all(SYS_STDERR, SHORT_USAGE);
                co_return 1;
            }
            (a == "--from-code" ? from : to) = args[++i];
        } else if (a[1] == '-') {
            co_await say("unknown option ", a);
            co_await write_all(SYS_STDERR, SHORT_USAGE);
            co_return 1;
        } else {
            // A short bundle, upstream's "csLlf:t:" plus an h.
            for (usize k = 1; k < a.size(); k++) {
                char c = a[k];
                if (c == 'c') {
                    opt_c = true;
                } else if (c == 's') {
                    opt_s = true;
                } else if (c == 'l') {
                    opt_l = true;
                } else if (c == 'h') {
                    opt_h = true;
                } else if (c == 'f' || c == 't') {
                    Str v = a.substr(k + 1);
                    if (v.empty()) {
                        if (i + 1 >= args.size()) {
                            co_await write_all(SYS_STDERR, SHORT_USAGE);
                            co_return 1;
                        }
                        v = args[++i];
                    }
                    (c == 'f' ? from : to) = v;
                    k                      = a.size();
                } else {
                    Buf<64> m;
                    m.put("iconv: illegal option -- ").put(c).put('\n');
                    co_await write_all(SYS_STDERR, m.str());
                    co_await write_all(SYS_STDERR, SHORT_USAGE);
                    co_return 1;
                }
            }
        }
    }

    // Before the tables: help works where none are installed.
    if (opt_h) {
        if ((co_await File::stdout().write(HELP)).is_err())
            co_return 1;
        co_return (co_await File::stdout().flush()).is_err() ? 1 : 0;
    }

    // Where the tables are. Everything below reads them.
    co_await citrus_prefix_init();
    if ((co_await citrus_preload()).is_err()) {
        Buf<256> m;
        m.put("iconv: no character tables under ").put(citrus_prefix()).put("/share/i18n\n");
        co_await write_all(SYS_STDERR, m.str());
        co_return 1;
    }

    if (opt_l) {
        if (opt_s || opt_c || !from.empty() || !to.empty()) {
            co_await say("-l is not allowed with other flags.");
            co_await write_all(SYS_STDERR, SHORT_USAGE);
            co_return 1;
        }
        String names;
        LIST_OUT = &names;
        __bsd_iconvlist(list_one, nullptr);
        LIST_OUT = nullptr;
        if ((co_await File::stdout().write(names.str())).is_err())
            co_return 1;
        co_return (co_await File::stdout().flush()).is_err() ? 1 : 0;
    }

    if (from.empty() && to.empty()) {
        co_await write_all(SYS_STDERR, SHORT_USAGE);
        co_return 1;
    }

    // A name has to be a C string, and Str is a view.
    Buf<128> fbuf, tbuf;
    fbuf.put(from).put('\0');
    tbuf.put(to).put('\0');

    iconv_t cd = (iconv_t)-1;
    if (Task<iconv_t> t = __bsd_iconv_open(tbuf.str().data(), fbuf.str().data()))
        cd = co_await t;
    if (cd == (iconv_t)-1) {
        Buf<192> m;
        m.put("iconv: iconv_open(").put(to).put(", ").put(from).put("): ");
        m.put(error_name(iconv_error(errno))).put('\n');
        co_await write_all(SYS_STDERR, m.str());
        co_return 1;
    }

    // The grid holds codepoints, not bytes, and decodes what is written to it
    // as UTF-8 — so a non-UTF-8 target on the console is mojibake by
    // construction. Upstream has nothing to say here because a Unix terminal
    // shows bytes.
    if (!to.empty() && to != "UTF-8" && to != "utf-8" && to != "UTF8") {
        Result<TtyInfo> tty = co_await tty_of(SYS_STDOUT);
        if (tty.is_ok() && tty.value().console)
            co_await say("warning: the console renders UTF-8; redirect to a file");
    }

    Bufs *bufs = (Bufs *)heap_alloc(sizeof(Bufs));
    if (!bufs) {
        co_await say("out of memory");
        co_return 1;
    }

    sig_catch(SIG_INT);

    File &out = File::stdout();
    if (out.reserve(SYS_READ_MAX).is_err()) {
        heap_free(bufs);
        co_return 1;
    }

    int res    = 0;
    i32 status = 0;

    if (rest.size() == 0) {
        File in = File::of(SYS_STDIN, FileMode::Read);
        res     = co_await do_conv(in, cd, bufs, opt_s, opt_c);
    } else {
        for (Str path : rest) {
            int r;
            if (path == "-") {
                File in = File::of(SYS_STDIN, FileMode::Read);
                r       = co_await do_conv(in, cd, bufs, opt_s, opt_c);
            } else {
                Result<File> f = Err(Error::NoMemory);
                if (Task<Result<File>> t = File::open(path))
                    f = co_await t;
                if (f.is_err()) {
                    // Upstream carries on to the next operand rather than
                    // stopping, for conformance.
                    co_await errln("iconv", path, f.error());
                    res = 1;
                    continue;
                }
                r = co_await do_conv(f.value(), cd, bufs, opt_s, opt_c);
                (void)co_await f.value().close();
            }
            if (r < 0) {
                res = r;
                break;
            }
            res |= r;
            // Between files the shift state starts again.
            (void)__bsd_iconv(cd, nullptr, nullptr, nullptr, nullptr);
        }
    }

    heap_free(bufs);
    __bsd_iconv_close(cd);

    if ((co_await out.flush()).is_err() && res == 0)
        res = 1;

    if (res == -130)
        status = 130;
    else if (res < 0)
        status = 1;
    else
        status = res == 0 ? 0 : 1;
    co_return status;
}
