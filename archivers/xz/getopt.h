// Minimal getopt_long for the xz CLI (no gnulib).
#pragma once

#include "kernel/args.h"
#include "kernel/types.h"

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
extern int optreset;

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2

// Parse a writable null-terminated argv (XZ_DEFAULTS / XZ_OPT only).
int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts,
                int *longindex);

// Parse Braam Args (views, not null-terminated). Call getopt_begin() first.
void getopt_begin(Args args);
int getopt_long_args(const char *optstring, const struct option *longopts, int *longindex);
