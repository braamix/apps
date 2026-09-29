// The Braam front end: reads each source the command line names, hands it to
// the core, and writes what comes back. Only this file awaits.
#include "driver.h"
#include "files.h"
#include "kernel/alloc.h"

namespace {

const char USAGE[] =
    "usage: as [options] file...\n"
    "  --module                 write a module, file.wasm, not an object\n"
    "  -o <file>                write there instead; one input only\n"
    "  --debug-names            name what has an id, in the name section\n"
    "  --tokens                 print the tokens, write nothing\n"
    "  --numbers                print typed literals' bits, write nothing\n"
    "  --tree                   print the syntax tree, write nothing\n"
    "  --resolved               print it after resolution, write nothing\n"
    "Each file.wat is written to file.o in the current directory.\n";

// Everything the front end holds, off the coroutine frame.
struct Front {
    AsArgs args;
    Diag diag;
    Vec<u8> image; // the assembled output
    Out out;       // for stdout
    String output; // where it goes
    bool cancelled = false;
};

void failed(Front &s, Str what, Str path, Error why)
{
    if (why == Error::Cancelled) {
        s.cancelled = true;
        return;
    }
    Out m;
    m.put(what).put(' ').put(path).put(": ").put(error_name(why));
    s.diag.error(m.str());
}

// Stderr, then the status: 130 for ^C, 1 for any other failure.
Task<i32> finish(Front &s)
{
    if (s.diag.text.oom) {
        s.diag.text.clear();
        s.diag.error("out of memory");
    }
    co_await say(SYS_STDERR, s.diag.text.str());
    if (s.cancelled)
        co_return 130;
    co_return s.diag.failed() ? 1 : 0;
}

// One file: read, assembled, written. An error is reported and the next file
// is still assembled.
Task<void> assemble_one(Front &s, Str path)
{
    bool dump = s.args.as.tokens || s.args.as.numbers || s.args.as.tree || s.args.as.resolved;
    if (!dump && !output_name(s.args, path, s.output, s.diag))
        co_return;
    Result<String> r = co_await slurp(path);
    if (r.is_err()) {
        failed(s, "cannot open", path, r.error());
        co_return;
    }
    if (dump) {
        s.out.clear();
        if (s.args.as.tokens)
            dump_tokens(path, r.value().str(), s.out, s.diag);
        else if (s.args.as.tree || s.args.as.resolved)
            dump_tree(path, r.value().str(), s.args.as.resolved, s.out, s.diag);
        else
            dump_numbers(path, r.value().str(), s.out, s.diag);
        if (s.out.oom)
            s.diag.error("out of memory");
        co_await say(SYS_STDOUT, s.out.str());
        co_return;
    }
    s.image.clear();
    if (!assemble(path, r.value().str(), s.args.as, s.image, s.diag))
        co_return;
    Str bytes(reinterpret_cast<const char *>(s.image.data()), s.image.size());
    Result<void> w = co_await spill(s.output.str(), bytes);
    if (w.is_err())
        failed(s, "cannot write", s.output.str(), w.error());
}

Task<i32> run(Front &s, Args args)
{
    s.diag.tool  = "as";
    s.diag.limit = 0;
    Vec<Str> words;
    for (usize i = 1; i < args.size(); i++)
        if (!words.push(args[i])) {
            s.diag.error("out of memory");
            co_return co_await finish(s);
        }
    if (!parse_args(words, s.args, s.diag))
        co_return co_await finish(s);
    if (s.args.help) {
        co_await say(SYS_STDOUT, USAGE);
        co_return 0;
    }
    for (Str path : s.args.inputs) {
        co_await assemble_one(s, path);
        if (s.cancelled)
            break;
    }
    co_return co_await finish(s);
}

} // namespace

Task<i32> proc_main(Args args)
{
    Front *s = heap_new<Front>();
    if (!s)
        co_return 1;
    i32 status = co_await run(*s, args);
    heap_delete(s);
    co_return status;
}
