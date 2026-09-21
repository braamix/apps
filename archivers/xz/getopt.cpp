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
