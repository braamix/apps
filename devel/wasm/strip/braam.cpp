// The Braam front end: reads each file the command line names, hands its bytes
// to the core, and writes what comes back. Only this file awaits.
#include "archive.h"
#include "driver.h"
#include "files.h"
#include "kernel/alloc.h"

namespace {

const char USAGE[] =
    "usage: strip [options] file...\n"
    "  -s, --strip-all          remove all custom sections but braam (the default)\n"
    "  -g, -S, --strip-debug    remove debug sections only\n"
    "  -R <name>, --remove-section=<name>\n"
    "                           remove that custom section too\n"
    "  --keep-section=<name>    keep that custom section\n"
    "  -o <file>                write there instead of in place; one input only\n";

// Everything the front end holds, off the coroutine frame.
struct Front {
    StripArgs args;
    Diag diag;
    Vec<u8> image; // the stripped module
    String tmp;    // <file>.strip
    bool cancelled = false;
};

// Records a failed file operation; false, so a caller can return it.
bool failed(Front &s, Str what, Str path, Error why)
{
    if (why == Error::Cancelled) {
        s.cancelled = true;
        return false;
    }
    Out m;
    m.put(what).put(' ').put(path).put(": ").put(error_name(why));
    s.diag.error(m.str());
    return false;
}

// Stderr, then the status: 130 for ^C, 1 for any other failure.
Task<i32> finish(Front &s)
{
    co_await say(SYS_STDERR, s.diag.text.str());
    if (s.cancelled)
        co_return 130;
    co_return s.diag.failed() ? 1 : 0;
}

// Replaces `path` with the image: written beside it, then renamed over it,
// so a failure anywhere leaves the original whole. A temporary name already
// taken is an error, not something to overwrite.
Task<bool> replace(Front &s, Str path)
{
    if (!s.tmp.assign(path) || !s.tmp.append(".strip")) {
        s.diag.error("out of memory");
        co_return false;
    }
    Str tmp = s.tmp.str();
    Str bytes(reinterpret_cast<const char *>(s.image.data()), s.image.size());
    Result<void> w = co_await spill(tmp, bytes, SYS_O_EXCL);
    if (w.is_err() && w.error() == Error::Exists)
        co_return failed(s, "cannot create", tmp, w.error());
    if (w.is_ok()) {
        Task<Result<void>> t = rename_path(tmp, path);
        w                    = t ? co_await t : Result<void>(Err(Error::NoMemory));
        if (w.is_ok())
            co_return true;
    }
    Error why = w.error();
    if (Task<Result<void>> t = remove_path(tmp, false))
        co_await t;
    co_return failed(s, "cannot write", path, why);
}

// One file: read, stripped, written. An error is reported and the next file
// is still stripped.
Task<void> strip_one(Front &s, Str path)
{
    Result<String> r = co_await slurp(path);
    if (r.is_err()) {
        failed(s, "cannot open", path, r.error());
        co_return;
    }
    const String &f = r.value();
    Bytes file(reinterpret_cast<const u8 *>(f.data()), f.size());
    Out err;
    s.image.clear();
    bool stripped = is_archive(file) ? strip_archive(path, file, s.args.strip, s.image, err)
                                     : strip_module(path, file, s.args.strip, s.image, err);
    if (!stripped) {
        s.diag.error(err.str());
        co_return;
    }
    if (s.args.output.empty()) {
        // Nothing removed: the file is left as it is.
        bool same = s.image.size() == file.size();
        for (usize k = 0; same && k < file.size(); k++)
            same = s.image[k] == file[k];
        if (!same)
            co_await replace(s, path);
        co_return;
    }
    Str bytes(reinterpret_cast<const char *>(s.image.data()), s.image.size());
    Result<void> w = co_await spill(s.args.output, bytes);
    if (w.is_err())
        failed(s, "cannot write", s.args.output, w.error());
}

Task<i32> run(Front &s, Args args)
{
    s.diag.tool = "strip";
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
        co_await strip_one(s, path);
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
