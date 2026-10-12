//
// C preprocessor: diagnostics and small string helpers.
//
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "intern.h"

//
// Whether to color diagnostics, as libutil's diag_use_color() decides for the other
// passes: by --color, else when stderr is a terminal, NO_COLOR is unset or empty and
// TERM is not "dumb".  The test is made once.
//
static int use_color(void)
{
    static int tty = -1;

    if (cpp.opt_color)
        return cpp.opt_color > 0;
    if (tty < 0) {
        const char *no_color = getenv("NO_COLOR");
        const char *term     = getenv("TERM");
        tty = isatty(2) && !(no_color && no_color[0]) && !(term && strcmp(term, "dumb") == 0);
    }
    return tty;
}

//
// Print one diagnostic line "file:line: label: message" to stderr, as the other
// passes do (docs/Technical_Reference.md, "Diagnostics"); with no line yet (a
// command-line error), "progname: label: message".  `label` is "error" or
// "warning".  Every error bumps cpp.exit_code so the program exits non-zero.
//
static void vreport_at(const char *file, int line, const char *label, const char *s, va_list ap)
{
    int color = use_color();

    if (color)
        fputs("\033[1m", stderr);
    if (line > 0)
        fprintf(stderr, "%s:%d:", (file && file[0]) ? file : "<stdin>", line);
    else
        fprintf(stderr, "%s:", cpp.prog_name ? cpp.prog_name : "cpp");
    if (color)
        fprintf(stderr, "\033[0m %s%s:\033[0m ",
                strcmp(label, "warning") == 0 ? "\033[1;35m" : "\033[1;31m", label);
    else
        fprintf(stderr, " %s: ", label);
    vfprintf(stderr, s, ap);
    fprintf(stderr, "\n");
    ++cpp.exit_code;
}

//
// Report at the current line: the line of the directive being processed, if any.
//
static void vreport(const char *label, const char *s, va_list ap)
{
    int line = cpp.line_no[cpp.inc_level];
    if (cpp.dir_line > 0 && cpp.dir_level == cpp.inc_level)
        line = cpp.dir_line;
    vreport_at(cpp.inc_file[cpp.inc_level], line, label, s, ap);
}

void pperror_at(const char *file, int line, const char *s, ...)
{
    va_list ap;

    va_start(ap, s);
    vreport_at(file, line, "error", s, ap);
    va_end(ap);
}

void exit_cpp(void)
{
    exit(cpp.exit_code ? 1 : 0);
}

//
// Report a preprocessor error (printf-style).  Counts toward the exit status.
//
void pperror(const char *s, ...)
{
    va_list ap;

    va_start(ap, s);
    vreport("error", s, ap);
    va_end(ap);
}

//
// Report an error found while evaluating a #if expression.  Same as pperror();
// kept separate so the parser has its own named reporting hook.
//
void parse_error(const char *s, ...)
{
    va_list ap;

    va_start(ap, s);
    vreport("error", s, ap);
    va_end(ap);
}

//
// Report a warning: print it like an error but leave the exit status alone.
// We save exit_code, force it to -1 so vreport's "++" lands back on 0, print,
// then restore the real count.
//
void ppwarn(const char *s, ...)
{
    int fail = cpp.exit_code;
    va_list ap;

    cpp.exit_code = -1;
    va_start(ap, s);
    vreport("warning", s, ap);
    va_end(ap);
    cpp.exit_code = fail;
}

//
// Turn a file path into just its directory, in place: chop everything after the
// last '/'.  A path with no '/' becomes "." (the current directory).  Returns s.
// Used to remember which directory an #include file came from.
//
char *dir_of(char *s)
{
    char *p = s;
    while (*p++) // walk to the terminating '\0'
        ;
    --p;
    while (p > s && *--p != '/') // walk back to the last '/'
        ;
    if (p == s)
        *p++ = '.'; // no '/' at all: the directory is "."
    *p = '\0';
    return (s);
}

//
// Copy a null-terminated string into the side buffer (cpp.side_ptr grows as a
// simple bump allocator) and return a pointer to the stored copy.  Used to keep
// file names, macro names and definition text alive after the scan buffer that
// held them is reused.
//
STATIC char *save_string(const char *s)
{
    char *old;

    old = cpp.side_ptr;
    while ((*cpp.side_ptr++ = *s++)) // copy including the '\0'
        ;
    return (old);
}

//
// Find character c in string s, like strchr(): return a pointer to the first
// occurrence, or null (0) if c is not present.
//
char *find_char(char *s, int c)
{
    while (*s)
        if (*s++ == c)
            return (--s);
    return (0);
}
