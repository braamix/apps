// A getopt_long for ar: see getopt.h.
#include "getopt.h"

#include <stdlib.h>
#include <string.h>

int optind   = 1;
int optopt   = 0;
char *optarg = nullptr;

namespace {

int nextchar  = 0;     // within argv[optind]; 0 before an argument is begun
int nopts     = 0;     // options at the front of argv, after the permutation
bool dashdash = false; // "--" follows them

bool is_option(const char *a)
{
    return a[0] == '-' && a[1] != '\0';
}

// Options to the front, then "--" if it was given, then the operands in
// their order; everything after "--" is an operand.
void permute(int argc, char **argv)
{
    char **opts = static_cast<char **>(malloc(2 * argc * sizeof(char *)));
    if (opts == nullptr)
        return;
    char **rest = opts + argc;
    char *dash  = nullptr;
    int no = 0, nr = 0, i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            dash = argv[i++];
            break;
        }
        if (is_option(argv[i]))
            opts[no++] = argv[i];
        else
            rest[nr++] = argv[i];
    }
    for (; i < argc; i++)
        rest[nr++] = argv[i];
    int k = 1;
    for (int j = 0; j < no; j++)
        argv[k++] = opts[j];
    if (dash)
        argv[k++] = dash;
    for (int j = 0; j < nr; j++)
        argv[k++] = rest[j];
    nopts    = no;
    dashdash = dash != nullptr;
    free(opts);
}

} // namespace

int getopt_long(int argc, char **argv, const char *optstring, const struct option *longopts,
                int *longindex)
{
    optarg = nullptr;
    if (optind == 1 && nextchar == 0)
        permute(argc, argv);
    if (optind > nopts) {
        if (dashdash && optind == nopts + 1)
            optind++;
        dashdash = false;
        return -1;
    }
    const char *a = argv[optind];

    if (nextchar == 0 && a[1] == '-') {
        const char *name = a + 2;
        size_t len       = strlen(name);
        int found = -1, prefixes = 0;
        for (int i = 0; longopts && longopts[i].name; i++) {
            if (strncmp(longopts[i].name, name, len) != 0)
                continue;
            if (strlen(longopts[i].name) == len) {
                found    = i;
                prefixes = 1;
                break;
            }
            found = i;
            prefixes++;
        }
        if (prefixes != 1)
            found = -1;
        optind++;
        if (found < 0) {
            warnx("unknown option -- %s", name);
            optopt = 0;
            return '?';
        }
        if (longindex)
            *longindex = found;
        if (longopts[found].flag) {
            *longopts[found].flag = longopts[found].val;
            return 0;
        }
        return longopts[found].val;
    }

    if (nextchar == 0)
        nextchar = 1;
    int c = (unsigned char)a[nextchar++];
    if (a[nextchar] == '\0') {
        optind++;
        nextchar = 0;
    }
    if (c == ':' || strchr(optstring, c) == nullptr) {
        warnx("illegal option -- %c", c);
        optopt = c;
        return '?';
    }
    return c;
}
