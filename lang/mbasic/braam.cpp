// The Braam platform: proc_main, the driver loop, and every co_await in the
// program.
//
// The interpreter below runs as plain C++ and hands the driver what it needs
// done (mbasic.h). Nothing under step() may block: a read, a write and a sleep
// are coroutines here, and a coroutine cannot be entered from a plain
// function. Making the statement loop itself one is worse -- a co_await is a
// call and not a tail call, so awaiting without suspending grows the native
// stack until it traps.
//
// This is simbesm's cpu_burst() shape (emulators/simbesm/machine.h), and the
// input half is adventure's (games/adventure/braam.cpp): a LineEditor when
// stdin is a console, an Input/LineReader when it is a pipe or a file.
#include "edit.h"
#include "epath.h"
#include "kernel/alloc.h"
#include "kernel/fmt.h"
#include "kernel/key.h"
#include "mbasic.h"
#include "proc/io.h"
#include "proc/opt.h"
#include "proc/rt.h"
#include "proc/usage.h"

namespace {

constexpr Str WHO = "mbasic";

constexpr Str USAGE =
    "Usage:\n"
    "    mbasic              interactive\n"
    "    mbasic <file>       run the program in <file> and exit\n"
    "    mbasic <script      read a session from stdin, as if typed\n"
    "\n"
    "Microsoft BASIC 1.1 for the 6502, written by Weiland & Gates, ported\n"
    "from the 1978 sources.\n"
    "A named file prints only what the program prints; INPUT reads stdin.\n"
    "A bare name not in this directory is looked for among the examples.\n";

// With a console the editor runs; a pipe or a file reads stdin instead. All of
// these outlive every read, so none sits in a coroutine frame. Built on first
// use and never torn down: the kernel drops the instance.
bool console; // stdin is a terminal, so a correctable line is possible
bool have_keys;
LineEditor *editor;
Input *input;
LineReader *lines;

// Script mode: the file's text, and how far the MAIN reads have got.
String *script;
usize script_at;

// ISCNTC's other half.
//
// Upstream polled the keyboard once per statement. That is a thing no program
// can do here -- key_read() blocks and there is no non-blocking form -- and a
// key ring has exactly one receiver, so the editor cannot read it while
// something else waits for a ^C.
//
// So the keyboard changes hands at the one boundary that matters. At the
// prompt it is ours, and the editor reads it. The moment a BASIC program runs
// long enough to yield, it goes back to the console, whose pump turns ^C into
// SIG_INT -- which the burst's park lets in, because a signal is delivered
// where a process parks. The line editor and ISCNTC therefore never want it at
// the same time, and neither loses a keystroke to the other.
bool holding;

Task<bool> keys_take(bool take)
{
    if (!console || take == holding)
        co_return holding;
    Result<Geometry> g = Err(Error::Perm);
    if (Task<Result<Geometry>> t = keys_claim(take))
        g = co_await t;
    if (g.is_ok())
        holding = take;
    co_return holding;
}

Task<void> input_init(Args paths)
{
    Result<TtyInfo> tty = Err(Error::Unsupported);
    if (Task<Result<TtyInfo>> t = tty_of(SYS_STDIN))
        tty = co_await t;

    // A script's own text does not come from stdin, so the editor is still
    // available for the INPUT statements in it.
    console = tty.is_ok() && tty.value().console;
    (void)paths;

    // A signal is delivered where a process parks, so the burst's park is what
    // makes a ^C during RUN reachable at all.
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    // Both, always: the claim is made at the first read rather than here,
    // because the shell gives the keyboard back on its way to running us and
    // a claim that raced it would answer Err(Perm) for the whole session. If
    // it fails anyway the cooked path is still there, which is also the path a
    // pipe and a named file take.
    if (console) {
        editor = heap_new<LineEditor>();
        if (Task<Result<void>> t = sig_catch(SIG_WINCH))
            co_await t;
    }
    input = heap_new<Input>(Args{}, SYS_STDIN, WHO);
    if (input)
        lines = heap_new<LineReader>(*input);
    co_return;
}

// The keyboard has to go back on every path, including the interrupted one, or
// the shell never reads again.
// The keyboard has to go back on every path, including the interrupted one, or
// the shell never reads again.
Task<void> input_done()
{
    co_await keys_take(false);
    have_keys = false;
    co_return;
}

// What OUTDO would have done, where a write is allowed. The channels go with
// it, so PRINT# is buffered exactly as PRINT is.
Task<void> drain(Interp &b)
{
    if (!b.out.empty()) {
        if (Task<Result<void>> t = write_all(SYS_STDOUT, b.out.str()))
            (void)co_await t;
        b.out.clear();
    }
    for (usize i = 0; i < b.chans.size(); i++) {
        Chan &c = b.chans[i];
        if (c.obuf.empty() || c.fd < 0)
            continue;
        if (Task<Result<void>> t = write_all(u32(c.fd), c.obuf.str()))
            (void)co_await t;
        c.obuf.clear();
    }
    co_return;
}

// INLIN. Upstream's contract was "read into BUF, backarrow deletes a
// character, @ deletes the line"; on the Apple it delegated the whole edit to
// the monitor. Delegating to the system's own editor is the same choice.
Task<void> serve_line(Interp &b)
{
    b.in_line.clear();
    b.in_end = InEnd::Line;

    // A MAIN line in script mode comes from the file, never from stdin, which
    // stays free for the INPUT statements in it.
    if (b.req.main && script) {
        Str t = script->str();
        if (script_at >= t.size()) {
            b.in_end = InEnd::Eof;
            co_return;
        }
        usize e = script_at;
        while (e < t.size() && t[e] != '\n')
            e++;
        usize n = e - script_at;
        if (n && t[script_at + n - 1] == '\r')
            n--;
        if (!b.in_line.assign(t.substr(script_at, n)))
            b.in_end = InEnd::Error;
        script_at = e < t.size() ? e + 1 : e;
        co_return;
    }

    if (b.req.chan) { // INPUT#: a line from a channel, not from the console
        Chan *c = b.chan_find(b.req.chan);
        if (!c) {
            b.in_end = InEnd::Error;
            co_return;
        }
        if (c->ipos >= c->ibuf.size()) {
            b.in_end = c->eof ? InEnd::Eof : InEnd::Error;
            co_return;
        }
        usize e = c->ipos;
        while (e < c->ibuf.size() && c->ibuf[e] != '\n')
            e++;
        usize n = e - c->ipos;
        if (n && c->ibuf[c->ipos + n - 1] == '\r')
            n--;
        if (!b.in_line.assign(Str(c->ibuf.data() + c->ipos, n)))
            b.in_end = InEnd::Error;
        c->ipos = e < c->ibuf.size() ? e + 1 : e;
        co_return;
    }

    // Err(Perm) if somebody else holds them, which a background job does not
    // get past either; then the cooked path below serves instead.
    have_keys = editor && co_await keys_take(true);
    if (have_keys) {
        Result<InLine> r = Err(Error::NoMemory);
        if (Task<Result<InLine>> t = editor->read_line())
            r = co_await t;
        (void)sig_take(SIG_INT); // a ^C the editor took is not also a BREAK
        if (r.is_err()) {
            b.in_end = InEnd::Eof;
            co_return;
        }
        if (r.value().how == LineEnd::Interrupt) {
            b.in_end = InEnd::Interrupt;
            b.trmpos = 0;
            co_return;
        }
        if (r.value().how == LineEnd::Eof) {
            b.in_end = InEnd::Eof;
            b.trmpos = 0;
            co_return;
        }
        if (!b.in_line.assign(r.value().text.str()))
            b.in_end = InEnd::Error;
        b.trmpos = 0; // the echoed newline, which upstream saw through OUTDO
        co_return;
    }

    if (!lines) {
        b.in_end = InEnd::Eof;
        co_return;
    }
    Result<bool> r = co_await lines->next(b.in_line);
    // A ^C abandons the read with nothing taken from it: the DEL again.
    if (r.is_err() && r.error() == Error::Intr && sig_take(SIG_INT)) {
        b.in_end = InEnd::Interrupt;
        co_return;
    }
    if (r.is_err() || !r.value())
        b.in_end = InEnd::Eof;
    co_return;
}

// GET: exactly one character, and no echo. Upstream reached the machine's
// CZGETL for it, which was INCHR without the line discipline.
Task<void> serve_char(Interp &b)
{
    b.in_char = 0;
    b.in_end  = InEnd::Line;

    have_keys = console && co_await keys_take(true);
    if (have_keys) {
        Result<KeyPress> r = Err(Error::NoMemory);
        if (Task<Result<KeyPress>> t = key_read())
            r = co_await t;
        if (r.is_err()) {
            b.in_end = r.error() == Error::Intr ? InEnd::Interrupt : InEnd::Eof;
            co_return;
        }
        const KeyPress &k = r.value();
        if ((k.mods & MOD_CTRL) && k.code == 'c') {
            (void)sig_take(SIG_INT);
            b.in_end = InEnd::Interrupt;
            co_return;
        }
        // GET yields the null string for a key with no character, which is
        // upstream's behaviour when nothing was ready.
        b.in_char = k.code < 128 ? u8(k.code) : 0;
        co_return;
    }

    if (!lines) {
        b.in_end = InEnd::Eof;
        co_return;
    }
    Result<String> r = co_await input->read();
    if (r.is_err() || r.value().empty()) {
        b.in_end = InEnd::Eof;
        co_return;
    }
    b.in_char = u8(r.value()[0]);
    co_return;
}

// LOAD, SAVE, OPEN and CLOSE. Upstream's were the machine's cassette or its
// KERNAL vectors; a program is LIST-format text here, which is what makes it
// portable between a browser tab and anything else.
Task<void> serve_file(Interp &b)
{
    FileReq &f = b.req.file;
    f.ok       = false;

    switch (f.op) {
    case FileOp::Load: {
        f.data.clear();
        Result<String> r = Err(Error::NoMemory);
        if (Task<Result<String>> t = read_file(f.name.str()))
            r = co_await t;
        // A bare name the working directory does not hold is looked for among
        // the examples the package ships. SAVE has no such fallback: the store
        // is read-only.
        if (r.is_err()) {
            String alt;
            if (!epath_file(f.name.str(), alt))
                co_return;
            if (Task<Result<String>> t = read_file(alt.str()))
                r = co_await t;
            if (r.is_err())
                co_return;
        }
        f.data = static_cast<String &&>(r.value());
        f.ok   = true;
        co_return;
    }
    case FileOp::Save: {
        Result<i32> fd = Err(Error::NoMemory);
        if (Task<Result<i32>> t = open_at(f.name.str(), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC))
            fd = co_await t;
        if (fd.is_err())
            co_return;
        Result<void> w = Err(Error::Io);
        if (Task<Result<void>> t = write_all(u32(fd.value()), f.data.str()))
            w = co_await t;
        if (Task<void> t = close_fd(u32(fd.value())))
            co_await t;
        f.ok = w.is_ok();
        co_return;
    }
    case FileOp::Open: {
        if (f.input) {
            Result<String> r = Err(Error::NoMemory);
            if (Task<Result<String>> t = read_file(f.name.str()))
                r = co_await t;
            if (r.is_err())
                co_return;
            f.data = static_cast<String &&>(r.value());
            f.fd   = -1;
            f.ok   = true;
            co_return;
        }
        Result<i32> fd = Err(Error::NoMemory);
        if (Task<Result<i32>> t = open_at(f.name.str(), SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC))
            fd = co_await t;
        if (fd.is_err())
            co_return;
        f.fd = fd.value();
        f.ok = true;
        co_return;
    }
    case FileOp::Close:
        if (f.fd >= 0)
            if (Task<void> t = close_fd(u32(f.fd)))
                co_await t;
        f.fd = -1;
        f.ok = true;
        co_return;
    }
    co_return;
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (help_asked(args))
        co_return co_await usage_asked(USAGE);

    OptParse parse(args, Opts{ "", "" });
    for (Opt o;;) {
        Result<bool> r = parse.next(o);
        if (r.is_err())
            co_return co_await usage_error(USAGE);
        if (!r.value())
            break;
    }
    Args rest = parse.rest();

    Interp *b = heap_new<Interp>(); // never on a coroutine frame
    if (!b)
        co_return 1;
    b->poke_space = static_cast<u8 *>(heap_alloc(65536));
    if (!b->poke_space)
        co_return 1;
    for (u32 i = 0; i < 65536; i++)
        b->poke_space[i] = 0;

    co_await input_init(rest);
    co_await epath_init(); // once, so LOAD's fallback costs no syscall

    if (rest.size() > 1)
        co_return co_await usage_error(USAGE);
    if (rest.size() == 1) {
        Str path         = rest[0];
        Result<String> r = Err(Error::NoMemory);
        if (Task<Result<String>> t = read_file(path))
            r = co_await t;
        String alt; // a bare name may be one of the shipped examples
        if (r.is_err() && epath_file(path, alt))
            if (Task<Result<String>> t = read_file(alt.str()))
                r = co_await t;
        if (r.is_err()) {
            Buf<320> m;
            m.put(WHO);
            m.put(": cannot read ");
            m.put(path);
            m.put('\n');
            if (Task<Result<void>> t = write_all(SYS_STDERR, m.str()))
                (void)co_await t;
            co_return 1;
        }
        script = heap_new<String>(static_cast<String &&>(r.value()));
        if (!script)
            co_return 1;
        b->script = true;
    }

    u32 cols = 0;
    if (Task<Result<TtyInfo>> t = tty_of(SYS_STDOUT)) {
        Result<TtyInfo> tty = co_await t;
        if (tty.is_ok())
            cols = tty.value().at.cols;
    }

    u32 last_yield = proc_now();
    Reason r       = b->start(cols, have_keys);
    for (;;) {
        // sig_take is a memory read, so this costs nothing per burst.
        if (sig_take(SIG_INT))
            b->brkflg = true;

        co_await drain(*b); // before the read, so a prompt is on screen first

        switch (r) {
        case Reason::Done:
            co_await input_done();
            co_return b->status;

        case Reason::Yield: {
            // The park. It is what makes ^C reachable at all -- a signal is
            // delivered where a process parks -- and it is what gives this
            // task's native stack back, since a co_await is a call and not a
            // tail call. uemacs's exec_yield(), one level up.
            //
            // A burst the clock did not tick over is faster than the clock can
            // measure, so wait a millisecond rather than spin: a process that
            // always asks for zero never lets its worker breathe.
            // And the moment the keyboard is not wanted for a prompt, it goes
            // back, so that a ^C reaches ISCNTC through the console pump.
            (void)co_await keys_take(false);

            u32 t_now  = proc_now();
            u32 wait   = (t_now == last_yield) ? 1 : 0;
            last_yield = t_now;
            if (Task<Result<void>> t = sleep_for(wait))
                (void)co_await t;
            break;
        }

        case Reason::NeedLine:
            co_await serve_line(*b);
            break;
        case Reason::NeedChar:
            co_await serve_char(*b);
            break;
        case Reason::NeedFile:
            co_await serve_file(*b);
            break;
        }

        r = b->step();
    }
}
