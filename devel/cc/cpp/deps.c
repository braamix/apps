//
// The dependency rule of -M, -MM, -MD and -MMD: the files the input included,
// as make's prerequisites of an object, in the format GCC writes.
//
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intern.h"

static int mode;          // DEPS_* of the option given, or 0
static int user_only;     // -MM, -MMD: leave out the system headers
static int phony;         // -MP
static int missing_ok;    // -MG
static const char *file;  // -MF
static const char *input; // the main file, for the default target

static char **targets; // -MT verbatim, -MQ already quoted
static int ntargets;
static char **deps; // each header, once
static int ndeps;

static char inc_system[MAXINC]; // the file at each include level is a system header

enum { DEPS_ONLY = 1, DEPS_TOO = 2 };

// Append s to a growing list.
static void push(char ***list, int *n, char *s)
{
    *list           = realloc(*list, (*n + 1) * sizeof **list);
    (*list)[(*n)++] = s;
}

// The name quoted for make: a blank escaped with '\' (and the '\'s before it
// doubled), '$' as '$$', '#' as '\#'.
static char *quote(const char *s)
{
    char *q = malloc(2 * strlen(s) + 1), *p = q;
    for (const char *c = s; *c; c++) {
        if (*c == ' ' || *c == '\t') {
            for (const char *b = c; b > s && b[-1] == '\\'; b--)
                *p++ = '\\';
            *p++ = '\\';
        } else if (*c == '$') {
            *p++ = '$';
        } else if (*c == '#') {
            *p++ = '\\';
        }
        *p++ = *c;
    }
    *p = '\0';
    return q;
}

int deps_option(int argc, char *argv[], int *i)
{
    const char *a = argv[*i];

    if (strcmp(a, "-M") == 0 || strcmp(a, "-MM") == 0) {
        mode      = DEPS_ONLY;
        user_only = a[2] == 'M';
        return 1;
    }
    if (strcmp(a, "-MD") == 0 || strcmp(a, "-MMD") == 0) {
        mode      = DEPS_TOO;
        user_only = a[2] == 'M';
        return 1;
    }
    if (strcmp(a, "-MP") == 0) {
        phony = 1;
        return 1;
    }
    if (strcmp(a, "-MG") == 0) {
        missing_ok = 1;
        return 1;
    }
    if (a[2] != 'F' && a[2] != 'T' && a[2] != 'Q')
        return 0;
    const char *val = a + 3;
    if (*val == '\0') {
        if (*i + 1 >= argc) {
            pperror("missing argument to '%s'", a);
            return 1;
        }
        val = argv[++*i];
    }
    if (a[2] == 'F')
        file = val;
    else
        push(&targets, &ntargets, a[2] == 'Q' ? quote(val) : (char *)val);
    return 1;
}

int deps_quiet(void)
{
    return mode == DEPS_ONLY;
}

// A path as GCC lists it: without a leading "./".
static void add(const char *path)
{
    while (path[0] == '.' && path[1] == '/')
        for (path += 2; *path == '/'; path++)
            ;
    for (int k = 0; k < ndeps; k++)
        if (strcmp(deps[k], path) == 0)
            return;
    push(&deps, &ndeps, strdup(path));
}

void deps_main(const char *path)
{
    input = strdup(path); // dir_of() cuts argv[i] down to the directory
}

void deps_include(int level, const char *path, int system)
{
    inc_system[level] = system || (level > 0 && inc_system[level - 1]);
    if (mode && !(user_only && inc_system[level]))
        add(path);
}

int deps_missing(const char *name)
{
    if (mode != DEPS_ONLY || !missing_ok)
        return 0;
    add(name);
    return 1;
}

struct text {
    char *s;
    size_t len, cap;
};

static void put(struct text *t, const char *s)
{
    size_t n = strlen(s);
    if (t->len + n + 1 > t->cap) {
        t->cap = 2 * (t->len + n + 1);
        t->s   = realloc(t->s, t->cap);
    }
    memcpy(t->s + t->len, s, n + 1);
    t->len += n;
}

// A name after others on a line, wrapped where the line would pass 72
// columns, as GCC's mkdeps does.
static unsigned put_name(struct text *t, const char *name, unsigned col)
{
    size_t size = strlen(name);
    if (col) {
        if (col + size > 72) {
            put(t, " \\\n");
            col = 0;
        }
        col++;
        put(t, " ");
    }
    put(t, name);
    return col + (unsigned)size;
}

// The input's base name, its suffix replaced by suf: "x.o" or "x.d".
static char *base_name(char suf)
{
    const char *base  = input ? input : "-";
    const char *slash = strrchr(base, '/');
    if (slash)
        base = slash + 1;
    char *name = malloc(strlen(base) + 3);
    strcpy(name, base);
    char *dot = strrchr(name, '.');
    if (!dot || dot == name)
        dot = name + strlen(name);
    dot[0] = '.';
    dot[1] = suf;
    dot[2] = '\0';
    return name;
}

coro(braam_call *) void deps_write(void)
{
    if (!mode || cpp.exit_code)
        return;
    if (ntargets == 0)
        push(&targets, &ntargets, quote(base_name('o')));

    struct text t = { 0 };
    unsigned col  = 0;
    for (int k = 0; k < ntargets; k++)
        col = put_name(&t, targets[k], col);
    put(&t, ":");
    col++;
    if (input)
        col = put_name(&t, quote(input), col);
    for (int k = 0; k < ndeps; k++)
        col = put_name(&t, quote(deps[k]), col);
    put(&t, "\n");
    for (int k = 0; phony && k < ndeps; k++) {
        put(&t, "\n");
        put_name(&t, quote(deps[k]), 0);
        put(&t, ":\n");
    }

    FILE *out = cpp.out_file;
    if (mode == DEPS_TOO || file) {
        if (!file)
            file = base_name('d');
        out = await fopen(file, "w");
        if (!out) {
            pperror("cannot create '%s': %s", file, strerror(errno));
            free(t.s);
            return;
        }
    }
    await fwrite(t.s, 1, t.len, out);
    if (out != cpp.out_file && await fclose(out) != 0)
        pperror("cannot write '%s': %s", file, strerror(errno));
    free(t.s);
}
