// The Braam platform: proc_main, the driver loop, and every co_await.
//
// The compiler and VM below run as plain C++ and hand the driver what they
// need done (c4.h). Nothing under c4_burst() may block: a read, a write and
// an open are coroutines here, and a coroutine cannot be entered from a plain
// function. Making the instruction loop itself one is worse -- a co_await is
// a call and not a tail call, so awaiting without suspending grows the native
// stack until it traps.
//
// This is simbesm's cpu_burst() shape (emulators/simbesm/machine.h).
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "c4.h"
#include "compat/cio.h"
#include "fs/path.h"
#include "kernel/alloc.h"
#include "kernel/string.h"
#include "proc/io.h"
#include "proc/rt.h"

namespace {

constexpr int POOLSZ = 256 * 1024;

// A namespace-scope String is not trivially destructible. Built on first use
// and never freed: the kernel drops the instance.
String *share;
String *joined;

Task<void> find_share()
{
    if (!share)
        share = heap_new<String>();
    if (!share)
        co_return;

    Str env = proc_env("C4_PREFIX");
    if (!env.empty()) {
        share->assign(env);
        co_return;
    }

    Result<String> link = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_link("/pkg/bin/c4"))
        link = co_await t;
    if (link.is_ok()) {
        Str dir  = path_dirname(link.value().str());
        Str root = path_dirname(dir);
        share->assign(root);
        share->append("/share");
        co_return;
    }

    Result<Vec<DirEntry>> ents = Err(Error::NoMemory);
    if (Task<Result<Vec<DirEntry>>> t = list_dir("/pkg/store"))
        ents = co_await t;
    if (ents.is_ok())
        for (const DirEntry &e : ents.value())
            if (e.name.str().starts_with("c4-")) {
                share->assign("/pkg/store/");
                share->append(e.name.str());
                share->append("/share");
                co_return;
            }
}

// Unix open flags to the kit's O_*: guest C4 programs pass 0 for O_RDONLY.
int unix_oflags(long long flags)
{
    int acc = (int)(flags & 3);
    int f   = O_RDONLY;
    if (acc == 1)
        f = O_WRONLY;
    else if (acc == 2)
        f = O_RDWR;
    if (flags & 64)
        f |= O_CREAT;
    if (flags & 128)
        f |= O_EXCL;
    if (flags & 512)
        f |= O_TRUNC;
    if (flags & 1024)
        f |= O_APPEND;
    return f;
}

Task<int> open_path(const char *path, int flags)
{
    int fd = co_await b_open(path, flags);
    if (fd >= 0)
        co_return fd;
    if (!path || strchr(path, '/') || !share || share->empty())
        co_return fd;
    if (!joined)
        joined = heap_new<String>();
    if (!joined || path_join(share->str(), Str(path), *joined).is_err())
        co_return fd;
    // path_join does not guarantee a NUL; b_open wants one.
    if (!joined->push('\0'))
        co_return fd;
    fd = co_await b_open(joined->data(), flags);
    joined->pop();
    co_return fd;
}

Task<int> flush()
{
    if (c4_olen <= 0)
        co_return 0;
    ssize_t n = co_await b_write(STDOUT_FILENO, c4_obuf, (size_t)c4_olen);
    c4_clear_out();
    if (n < 0)
        co_return errno == EINTR ? 130 : -1;
    co_return 0;
}

char **copy_argv(Args args)
{
    int argc    = (int)args.size();
    char **argv = (char **)malloc((size_t)(argc + 1) * sizeof(char *));
    if (!argv)
        return nullptr;
    for (int i = 0; i < argc; i++) {
        usize n = args[i].size();
        argv[i] = (char *)malloc(n + 1);
        if (!argv[i])
            return nullptr;
        memcpy(argv[i], args[i].data(), n);
        argv[i][n] = 0;
    }
    argv[argc] = nullptr;
    return argv;
}

// The VM word is 8 bytes and a pointer is 4, so guest `++argv` adds 8. A
// host `char **` would make `*argv` load two slots at once.
char **guest_argv(int argc, char **argv)
{
    long long *w = (long long *)malloc((size_t)(argc + 1) * sizeof(long long));
    if (!w)
        return nullptr;
    for (int i = 0; i < argc; i++)
        w[i] = (long long)(unsigned long)argv[i];
    w[argc] = 0;
    return (char **)w;
}

} // namespace

Task<i32> proc_main(Args args)
{
    int argc    = (int)args.size();
    char **argv = copy_argv(args);
    if (!argv)
        co_return -1;

    --argc;
    ++argv;
    if (argc > 0 && **argv == '-' && (*argv)[1] == 's') {
        src = 1;
        --argc;
        ++argv;
    }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') {
        debug = 1;
        --argc;
        ++argv;
    }
    if (argc < 1) {
        co_await b_printf("usage: c4 [-s] [-d] file ...\n");
        co_return -1;
    }

    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    if (Task<void> t = find_share())
        co_await t;

    if (c4_alloc(POOLSZ) < 0) {
        if (Task<int> t = flush())
            co_await t;
        co_return -1;
    }

    int fd = co_await open_path(*argv, O_RDONLY);
    if (fd < 0) {
        co_await b_printf("could not open(%s)\n", *argv);
        co_return -1;
    }

    char *text = (char *)malloc(POOLSZ);
    if (!text) {
        co_await b_printf("could not malloc(%d) source area\n", POOLSZ);
        co_await b_close(fd);
        co_return -1;
    }
    int n = (int)co_await b_read(fd, text, POOLSZ - 1);
    co_await b_close(fd);
    if (n <= 0) {
        co_await b_printf("read() returned %d\n", n);
        co_return -1;
    }
    text[n] = 0;

    c4_kwinit();
    c4_set_source(text);
    int rc = c4_compile();
    if (Task<int> t = flush()) {
        int f = co_await t;
        if (f)
            co_return f;
    }
    if (rc < 0)
        co_return (i32) c4_status;
    if (src)
        co_return 0;

    char **gargv = guest_argv(argc, argv);
    if (!gargv)
        co_return -1;
    c4_vmsetup(argc, gargv);

    for (;;) {
        int need = c4_burst();
        if (Task<int> t = flush()) {
            int f = co_await t;
            if (f)
                co_return f;
        }
        if (need == C4_TICK) {
            if (Task<Result<void>> t = sleep_for(0)) {
                Result<void> r = co_await t;
                if (r.is_err() && (r.error() == Error::Cancelled ||
                                   (r.error() == Error::Intr && sig_take(SIG_INT))))
                    co_return 130;
            }
            continue;
        }
        if (need == C4_OPEN) {
            int g = co_await open_path(c4_sys.path, unix_oflags(c4_sys.flags));
            c4_set_a(g);
            continue;
        }
        if (need == C4_READ) {
            ssize_t g = co_await b_read((int)c4_sys.fd, c4_sys.buf, (size_t)c4_sys.n);
            c4_set_a(g);
            continue;
        }
        if (need == C4_CLOSE) {
            int g = co_await b_close((int)c4_sys.fd);
            c4_set_a(g);
            continue;
        }
        if (need == C4_EXIT)
            co_return (i32) c4_status;
        co_return (i32) c4_status;
    }
}
