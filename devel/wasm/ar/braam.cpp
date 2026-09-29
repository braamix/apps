// The Braam front end: reads every file ar will look at, runs ar over the
// bytes, then writes what it made and what it said. Only this file awaits.
#include <errno.h>
#include <string.h>

#include "ar.h"
#include "archive.h"
#include "compat/cerr.h"
#include "files.h"
#include "kernel/alloc.h"
#include "proc/opt.h"
#include "proc/usage.h"

extern const char AR_USAGE[] =
    "Usage:\n"
    "    ar -r [-cuvsSU] [-a <member> | -b <member>] <archive> <file>...\n"
    "    ar -q [-cvsSU] <archive> <file>...\n"
    "    ar -d [-vsS] <archive> <member>...\n"
    "    ar -m [-vsS] [-a <member> | -b <member>] <archive> <member>...\n"
    "    ar -t [-v] <archive> [<member>...]\n"
    "    ar -p [-v] <archive> [<member>...]\n"
    "    ar -x [-uvCo] <archive> [<member>...]\n"
    "    ar -s <archive>\n"
    "The first letter is what to do, and its dash may be left out: ar rc libx.a *.o\n"
    "    -r    add files, replacing members of the same name in their place\n"
    "    -q    append files, without looking for members of the same name\n"
    "    -d    delete members\n"
    "    -m    move members, to the end or to -a's or -b's place\n"
    "    -t    list members\n"
    "    -p    print members to standard output\n"
    "    -x    extract members into the current directory; all, if none named\n"
    "    -s    write the symbol table\n"
    "Options:\n"
    "    -a <member>    put the files after that member\n"
    "    -b <member>    put the files before that member\n"
    "    -c    create the archive without a warning\n"
    "    -u    only what is newer: a file than its member, or a member than its file\n"
    "    -v    say what is done to each member; with -t, list as ls -l does\n"
    "    -C    extract nothing that is already there\n"
    "    -o    keep the members' dates; files have none here, so it warns\n"
    "    -s    write the symbol table; the default\n"
    "    -S    leave the symbol table out\n"
    "    -U    keep each file's date; members are dated 0 without it\n";

namespace {

// Everything the front end holds, off the coroutine frame.
struct Front {
    bsdar ar{};
    Vec<char *> argv;
    Vec<String> texts; // what was read; ar's views point into them
    String tmp;        // <archive>.ar
    bool cancelled = false;
};

// Looks at `path` once: whether it is there, what it is, and its bytes
// when `read` and it is an ordinary file.
Task<void> look(Front &s, const char *path, bool read)
{
    if (s.cancelled || ar_lookup(&s.ar, path)->error != ENOENT)
        co_return;
    for (const ar_file &f : s.ar.files)
        if (strcmp(f.path, path) == 0)
            co_return;
    ar_file f{};
    f.path             = path;
    Result<FileInfo> r = co_await stat_of(Str(path));
    if (r.is_err()) {
        if (r.error() == Error::Cancelled)
            s.cancelled = true;
        f.error = errno_of(r.error());
        s.ar.files.push(f);
        co_return;
    }
    f.regular = r.value().kind == SYS_KIND_FILE;
    f.mtime   = time_t(r.value().mtime / 1000);
    if (f.regular && read) {
        Result<String> d = co_await slurp(Str(path));
        if (d.is_err()) {
            if (d.error() == Error::Cancelled)
                s.cancelled = true;
            f.error = errno_of(d.error());
        } else {
            f.data = Bytes(reinterpret_cast<const u8 *>(d.value().data()), d.value().size());
            s.texts.push(move(d.value()));
        }
    }
    s.ar.files.push(f);
}

// What ar will look at: the archives, the files to add, and for -x the
// names of the members, which may be there already.
Task<void> gather(Front &s)
{
    bsdar &ar = s.ar;
    if (ar.ranlib) {
        for (int i = 0; i < ar.argc; i++)
            co_await look(s, ar.argv[i], true);
        co_return;
    }
    co_await look(s, ar.filename, true);
    if (ar.mode == 'q' || ar.mode == 'r')
        for (int i = 0; i < ar.argc; i++)
            co_await look(s, ar.argv[i], true);
    if (ar.mode == 'x') {
        ar_file *f = ar_lookup(&ar, ar.filename);
        Vec<Member> members;
        Vec<ArchiveSymbol> index;
        Out why;
        if (f->error == 0)
            read_archive(Str(ar.filename), f->data, members, index, why);
        for (const Member &m : members) {
            char *name = strndup(m.name.data(), m.name.size());
            if (name)
                co_await look(s, name, false);
        }
    }
}

// Replaces the archive at `path`: written beside it, then renamed over it,
// so a failure anywhere leaves the original whole.
Task<Result<void>> replace(Front &s, Str path, Str bytes)
{
    if (!s.tmp.assign(path) || !s.tmp.append(".ar"))
        co_return Err(Error::NoMemory);
    Str tmp        = s.tmp.str();
    Result<void> w = co_await spill(tmp, bytes, SYS_O_EXCL);
    if (w.is_err())
        co_return w;
    Task<Result<void>> t = rename_path(tmp, path);
    w                    = t ? co_await t : Result<void>(Err(Error::NoMemory));
    if (w.is_err())
        if (Task<Result<void>> u = remove_path(tmp, false))
            co_await u;
    co_return w;
}

// The files ar made. A failure is reported in ar's manner.
Task<int> perform(Front &s, int status)
{
    for (const ar_write &w : s.ar.writes) {
        Str bytes(reinterpret_cast<const char *>(w.data.data()), w.data.size());
        Result<void> r = w.archive ? co_await replace(s, Str(w.path), bytes)
                                   : co_await spill(Str(w.path), bytes);
        if (r.is_ok())
            continue;
        if (r.error() == Error::Cancelled) {
            s.cancelled = true;
            co_return 130;
        }
        if (w.archive)
            bsdar_warnc(&s.ar, errno_of(r.error()), "Failed to open '%s'", w.path);
        else
            bsdar_warnc(&s.ar, errno_of(r.error()), "Can't create '%s'", w.path);
        status = 1;
    }
    co_return status;
}

Task<i32> run(Front &s, Args args)
{
    bsdar &ar = s.ar;
    TAILQ_INIT(&ar.v_obj);
    for (usize i = 0; i < args.size(); i++) {
        char *a = strndup(args[i].data(), args[i].size());
        if (!a || !s.argv.push(a))
            co_return 1;
    }
    if (!s.argv.push(nullptr))
        co_return 1;
    ar.progname = args.size() ? bsdar_basename(s.argv[0]) : "ar";

    int status = ar_options(&ar, int(args.size()), s.argv.data());
    if (status < 0) {
        if (Result<Clock> c = co_await clock_now(); c.is_ok()) {
            ar.tz = long(c.value().tz_min) * 60;
        }
        co_await gather(s);
        if (s.cancelled)
            co_return 130;
        status = ar_run(&ar);
        if (!ar.stopped)
            status = co_await perform(s, status);
        if (s.cancelled)
            co_return 130;
    }
    for (const ar_chunk &c : ar.output)
        if ((co_await say(u32(c.fd), c.text.str())).is_err())
            break;
    co_return status;
}

} // namespace

Task<i32> proc_main(Args args)
{
    if (args.size() == 1 || help_asked(args))
        co_return co_await usage_asked(AR_USAGE);
    Front *s = heap_new<Front>();
    if (!s)
        co_return 1;
    i32 status = co_await run(*s, args);
    heap_delete(s);
    co_return status;
}
