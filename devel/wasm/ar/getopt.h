// getopt_long(3), as much of it as ar asks for: flags with no arguments,
// clustered, and long options with none. Operands are permuted after the
// options, as FreeBSD's permutes them; "--" ends the options.
#pragma once

extern int optind;
extern int optopt;
extern char *optarg;

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

#define no_argument 0

int getopt_long(int argc, char **argv, const char *optstring, const struct option *longopts,
                int *longindex);

// What getopt_long says of a bad option, as warnx(3) would say it.
void warnx(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
