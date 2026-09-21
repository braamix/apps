// Command-line parsing and gzip_main for the FreeBSD gzip port.
#include <string.h>

#include "braam.h"
#include "kernel/args.h"
#include "kernel/vec.h"
#include "proc/time.h"

extern suffixes_t suffixes[];
extern const int gzip_num_suffixes;
static const int SUFFIX_MAXLEN = 30;

static Task<i32> parse_options(Args args, Vec<Str> &paths, const char *progname)
{
    bool endopts = false;
    for (usize i = 1; i < args.size(); i++) {
        Str a = args[i];
        if (endopts || a.size() < 2 || a[0] != '-') {
            if (!paths.push(a))
                co_return 2;
            continue;
        }
        if (a == "--") {
            endopts = true;
            continue;
        }
        if (a[1] == '-') {
            if (a == "--stdout" || a == "--to-stdout")
                cflag = 1;
            else if (a == "--decompress" || a == "--uncompress")
                dflag = 1;
            else if (a == "--force")
                fflag = 1;
            else if (a == "--help")
                co_await usage();
            else if (a == "--keep")
                kflag = 1;
            else if (a == "--list") {
                lflag = 1;
                dflag = 1;
            } else if (a == "--no-name") {
                nflag = 1;
                Nflag = 0;
            } else if (a == "--name") {
                nflag = 0;
                Nflag = 1;
            } else if (a == "--quiet")
                qflag = 1;
            else if (a == "--recursive")
                rflag = 1;
            else if (a == "--test") {
                cflag = 1;
                tflag = 1;
                dflag = 1;
            } else if (a == "--verbose")
                vflag = 1;
            else if (a == "--version")
                co_await display_version();
            else if (a == "--fast")
                numflag = 1;
            else if (a == "--best")
                numflag = 9;
            else if (a == "--ascii")
                co_await b_fprintf(stderr, "%s: option --ascii ignored on this system\n", progname);
            else if (a == "--license")
                co_await display_license();
            else if (a.starts_with("--suffix=")) {
                Str opt = a.substr(9);
                int len = (int)opt.size();
                if (len != 0) {
                    if (len > SUFFIX_MAXLEN) {
                        maybe_errx("incorrect suffix: '%s': too long", opt.data());
                        co_return 2;
                    }
                    suffixes[0].zipped = opt.data();
                    suffixes[0].ziplen = len;
                } else {
                    suffixes[gzip_num_suffixes - 1].zipped = "";
                    suffixes[gzip_num_suffixes - 1].ziplen = 0;
                }
            } else
                co_await usage();
            continue;
        }
        for (usize k = 1; k < a.size(); k++) {
            char ch = a[k];
            if (ch >= '1' && ch <= '9')
                numflag = ch - '0';
            else if (ch == 'c')
                cflag = 1;
            else if (ch == 'd')
                dflag = 1;
            else if (ch == 'f')
                fflag = 1;
            else if (ch == 'h')
                co_await usage();
            else if (ch == 'k')
                kflag = 1;
            else if (ch == 'l') {
                lflag = 1;
                dflag = 1;
            } else if (ch == 'L')
                co_await display_license();
            else if (ch == 'N') {
                nflag = 0;
                Nflag = 1;
            } else if (ch == 'n') {
                nflag = 1;
                Nflag = 0;
            } else if (ch == 'q')
                qflag = 1;
            else if (ch == 'r')
                rflag = 1;
            else if (ch == 'S') {
                Str opt = a.substr(k + 1);
                if (opt.empty() && i + 1 < args.size())
                    opt = args[++i];
                int len = (int)opt.size();
                if (len != 0) {
                    if (len > SUFFIX_MAXLEN) {
                        maybe_errx("incorrect suffix: '%s': too long", opt.data());
                        co_return 2;
                    }
                    suffixes[0].zipped = opt.data();
                    suffixes[0].ziplen = len;
                } else {
                    suffixes[gzip_num_suffixes - 1].zipped = "";
                    suffixes[gzip_num_suffixes - 1].ziplen = 0;
                }
                break;
            } else if (ch == 't') {
                cflag = 1;
                tflag = 1;
                dflag = 1;
            } else if (ch == 'V')
                co_await display_version();
            else if (ch == 'v')
                vflag = 1;
            else
                co_await usage();
        }
    }
    co_return 0;
}

Task<i32> gzip_main(Args args)
{
    const char *progname = gzip_progname(args.size() ? args[0] : Str("gzip"));

    if (Task<Result<Clock>> c = clock_now()) {
        Result<Clock> r = co_await c;
        if (r.is_ok())
            gzip_set_time((time_t)(r.value().epoch_ms / 1000));
    }

    if (strcmp(progname, "gunzip") == 0)
        dflag = 1;
    else if (strcmp(progname, "zcat") == 0 || strcmp(progname, "gzcat") == 0)
        dflag = cflag = 1;

    Vec<Str> paths;
    int st = co_await parse_options(args, paths, progname);
    if (st != 0)
        co_return st;

    if (paths.size() == 0) {
        if (dflag)
            co_await handle_stdin();
        else
            co_await handle_stdout();
    } else {
        for (usize i = 0; i < paths.size(); i++) {
            char pathbuf[512];
            usize n = paths[i].size();
            if (n >= sizeof pathbuf)
                n = sizeof pathbuf - 1;
            memcpy(pathbuf, paths[i].data(), n);
            pathbuf[n] = '\0';
            co_await handle_pathname(pathbuf);
        }
    }

    if (qflag == 0 && lflag && paths.size() > 1)
        co_await print_list(-1, 0, "(totals)", 0);

    co_return exit_value;
}
