/*
 * <getopt.h> — getopt_long, as BSD's and GNU's (getopt_long.c, from FreeBSD).
 * Non-options are moved after the options unless POSIXLY_CORRECT is set or the
 * option string begins with '+'.
 */
#ifndef _GETOPT_H
#define _GETOPT_H

#define no_argument       0
#define required_argument 1
#define optional_argument 2

struct option {
    const char *name; /* the long option, without its dashes */
    int has_arg;      /* no_argument, required_argument or optional_argument */
    int *flag;        /* where to store val, or NULL to return it */
    int val;
};

extern char *optarg;
extern int optind, opterr, optopt, optreset;

int getopt(int argc, char *const argv[], const char *optstring);
int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts,
                int *longindex);
int getopt_long_only(int argc, char *const argv[], const char *optstring,
                     const struct option *longopts, int *longindex);

#endif /* _GETOPT_H */
