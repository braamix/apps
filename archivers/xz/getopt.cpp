// Minimal getopt_long for xz (BSD-style, no gnulib).
#include "getopt.h"

#include <string.h>

#include "private.h"

char *optarg = NULL;
int optind   = 1;
int opterr   = 1;
int optopt   = 0;
int optreset = 0;

static char *place = NULL;

static Args go_args;
static char go_prog[256];
static char go_opt[512];
static char go_optbuf[4096];

static void copy_str(char *dst, usize cap, Str s)
{
    if (cap == 0)
        return;
    usize n = s.size();
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, s.data(), n);
    dst[n] = '\0';
}

static void set_optarg_str(Str s)
{
    copy_str(go_optbuf, sizeof go_optbuf, s);
    optarg = go_optbuf;
}

void getopt_begin(Args args)
{
    go_args = args;
    copy_str(go_prog, sizeof go_prog, args.size() ? args[0] : Str("xz"));
    optind   = 1;
    place    = NULL;
    optreset = 0;
}

int getopt_long_args(const char *optstring, const struct option *longopts, int *longindex)
{
    if (optreset) {
        optind   = 1;
        place    = NULL;
        optreset = 0;
    }

    if (longindex)
        *longindex = -1;

    const int argc = (int)go_args.size();
    if (optind >= argc)
        return -1;

    Str arg = go_args[(usize)optind];
    if (arg.empty() || arg[0] != '-' || arg.size() == 1)
        return -1;

    if (arg.size() == 2 && arg[1] == '-') {
        ++optind;
        return -1;
    }

    if (arg.size() >= 2 && arg[1] == '-') {
        Str name = arg.substr(2);
        usize eq = name.find('=');
        Str optname;
        if (eq == Str::npos)
            optname = name;
        else
            optname = name.substr(0, eq);

        for (int i = 0; longopts[i].name != NULL; ++i) {
            Str ln(longopts[i].name);
            if (ln.size() != optname.size() || optname != ln)
                continue;

            if (longindex)
                *longindex = i;

            ++optind;
            if (eq != Str::npos) {
                if (longopts[i].has_arg == no_argument)
                    return '?';
                set_optarg_str(name.substr(eq + 1));
            } else if (longopts[i].has_arg == required_argument) {
                if (optind >= argc)
                    return '?';
                set_optarg_str(go_args[(usize)optind++]);
            } else if (longopts[i].has_arg == optional_argument) {
                optarg = NULL;
            }

            if (longopts[i].flag) {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }
        if (opterr) {
            copy_str(go_opt, sizeof go_opt, arg);
            message(V_ERROR, "%s: unrecognized option '%s'", go_prog, go_opt);
        }
        ++optind;
        return '?';
    }

    static Str short_arg;
    static usize short_off;

    if (place == NULL) {
        short_arg = arg;
        short_off = 1;
    }

    if (short_off >= short_arg.size()) {
        ++optind;
        place = NULL;
        return getopt_long_args(optstring, longopts, longindex);
    }

    char c = short_arg[short_off++];
    if (short_off >= short_arg.size()) {
        ++optind;
        place = NULL;
    } else {
        place = (char *)1;
    }

    const char *p = strchr(optstring, c);
    if (p == NULL) {
        optopt = c;
        if (opterr)
            message(V_ERROR, "%s: invalid option -- '%c'", go_prog, c);
        return '?';
    }

    if (p[1] == ':') {
        Str rest = short_arg.substr(short_off);
        if (!rest.empty()) {
            set_optarg_str(rest);
            short_off = short_arg.size();
            ++optind;
            place = NULL;
        } else if (optind < argc) {
            set_optarg_str(go_args[(usize)optind++]);
            place = NULL;
        } else {
            optopt = c;
            if (opterr)
                message(V_ERROR, "%s: option requires an argument -- '%c'", go_prog, c);
            return '?';
        }
    }

    return c;
}

int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts,
                int *longindex)
{
    if (optreset) {
        optind   = 1;
        place    = NULL;
        optreset = 0;
    }

    if (longindex)
        *longindex = -1;

    if (optind >= argc)
        return -1;

    char *arg = argv[optind];
    if (arg == NULL || arg[0] != '-' || arg[1] == '\0')
        return -1;

    if (arg[1] == '-' && arg[2] == '\0') {
        ++optind;
        return -1;
    }

    if (arg[1] == '-') {
        const char *name = arg + 2;
        char *eq         = strchr(arg, '=');
        size_t namelen   = eq ? (size_t)(eq - name) : strlen(name);

        for (int i = 0; longopts[i].name != NULL; ++i) {
            if (strlen(longopts[i].name) != namelen ||
                strncmp(longopts[i].name, name, namelen) != 0)
                continue;

            if (longindex)
                *longindex = i;

            optind++;
            if (eq) {
                if (longopts[i].has_arg == no_argument)
                    return '?';
                optarg = eq + 1;
            } else if (longopts[i].has_arg == required_argument) {
                if (optind >= argc)
                    return '?';
                optarg = argv[optind++];
            } else if (longopts[i].has_arg == optional_argument) {
                optarg = NULL;
            }

            if (longopts[i].flag) {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }
        if (opterr)
            message(V_ERROR, "%s: unrecognized option '%s'", argv[0], arg);
        optind++;
        return '?';
    }

    if (place == NULL || *place == '\0')
        place = arg + 1;

    char c = *place++;
    if (*place == '\0') {
        ++optind;
        place = NULL;
    }

    const char *p = strchr(optstring, c);
    if (p == NULL) {
        optopt = c;
        if (opterr)
            message(V_ERROR, "%s: invalid option -- '%c'", argv[0], c);
        return '?';
    }

    if (p[1] == ':') {
        if (*place != '\0') {
            optarg = place;
            place  = NULL;
            ++optind;
        } else if (optind < argc) {
            optarg = argv[optind++];
        } else {
            optopt = c;
            if (opterr)
                message(V_ERROR, "%s: option requires an argument -- '%c'", argv[0], c);
            return '?';
        }
    }

    return c;
}
