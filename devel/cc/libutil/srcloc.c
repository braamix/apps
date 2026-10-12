//
// Source locations and the prefix of a diagnostic.
//
#include "srcloc.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

SrcLoc diag_loc;
const char *diag_progname;
int diag_errors;
int diag_max_errors;
int diag_show_source = 1;
int diag_color       = -1;
jmp_buf *diag_recover;

SrcLoc diag_enter(SrcLoc loc)
{
    SrcLoc saved = diag_loc;
    if (loc.line > 0) {
        diag_loc = loc;
    }
    return saved;
}

//
// The interned names. They take plain malloc rather than xalloc: they must outlive
// the xfree_all() a test fixture calls between tests, and must not count as a leak
// in xreport_lost_memory(). A unit names only a few files, so a list will do.
//
typedef struct Name {
    struct Name *next;
    char text[1]; // dynamically sized
} Name;

static Name *names;

const char *srcloc_intern(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (const Name *n = names; n; n = n->next) {
        if (strcmp(n->text, name) == 0) {
            return n->text;
        }
    }
    size_t len = strlen(name);
    Name *n    = malloc(sizeof(Name) + len);
    if (!n) {
        abort();
    }
    memcpy(n->text, name, len + 1);
    n->next = names;
    names   = n;
    return n->text;
}

int diag_use_color(FILE *f)
{
    static int tty = -1; // the auto decision, made once
    if (diag_color >= 0) {
        return diag_color;
    }
    if (tty < 0) {
        const char *no_color = getenv("NO_COLOR");
        const char *term     = getenv("TERM");
        tty = isatty(fileno(f)) && !(no_color && no_color[0]) && !(term && strcmp(term, "dumb") == 0);
    }
    return tty;
}

int diag_set_color(const char *when)
{
    if (strcmp(when, "always") == 0) {
        diag_color = 1;
    } else if (strcmp(when, "never") == 0) {
        diag_color = 0;
    } else if (strcmp(when, "auto") == 0) {
        diag_color = -1;
    } else {
        diag_error((SrcLoc){ 0 }, "invalid argument '%s' to '--color'", when);
        return -1;
    }
    return 0;
}

//
// The escape that starts the color of a kind, as GCC paints it.
//
static const char *kind_color(const char *kind)
{
    if (strcmp(kind, "warning") == 0) {
        return "\033[1;35m";
    }
    if (strcmp(kind, "note") == 0) {
        return "\033[1;36m";
    }
    return "\033[1;31m";
}

void diag_print_prefix(FILE *f, SrcLoc loc, const char *kind)
{
    int color = diag_use_color(f);
    if (loc.line > 0 || diag_progname) {
        if (color) {
            fputs("\033[1m", f);
        }
        if (loc.line > 0) {
            fprintf(f, "%s:%d:", loc.file ? loc.file : "<input>", loc.line);
            if (loc.col > 0) {
                fprintf(f, "%d:", loc.col);
            }
        } else {
            const char *slash = strrchr(diag_progname, '/');
            fprintf(f, "%s:", slash ? slash + 1 : diag_progname);
        }
        fputs(color ? "\033[0m " : " ", f);
    }
    if (color) {
        fprintf(f, "%s%s:\033[0m ", kind_color(kind), kind);
    } else {
        fprintf(f, "%s: ", kind);
    }
}

//
// Print the source line of loc and a caret under its column, as GCC does:
//
//          3 |     int x = 12abc;
//            |             ^
//
// The file is read again by name; when it can't be, nothing is printed. The caret
// line repeats each tab before the column, so the caret lines up whatever the tab
// width. A column past the end of the line leaves the caret out.
//
void diag_print_source(FILE *f, SrcLoc loc)
{
    if (!diag_show_source || loc.line <= 0 || !loc.file || !loc.file[0]) {
        return;
    }
    FILE *src = fopen(loc.file, "r");
    if (!src) {
        return;
    }
    int c = 0;
    for (int line = 1; line < loc.line; line++) {
        while ((c = getc(src)) != EOF && c != '\n')
            ;
        if (c == EOF) {
            fclose(src);
            return;
        }
    }
    long start = ftell(src);
    int len    = 0;
    fprintf(f, "%5d | ", loc.line);
    while ((c = getc(src)) != EOF && c != '\n') {
        if (c != '\r') {
            fputc(c, f);
        }
        len++;
    }
    fputc('\n', f);
    if (loc.col > 0 && loc.col <= len + 1 && fseek(src, start, SEEK_SET) == 0) {
        fprintf(f, "      | ");
        for (int col = 1; col < loc.col; col++) {
            fputc(getc(src) == '\t' ? '\t' : ' ', f);
        }
        fputs(diag_use_color(f) ? "\033[1;32m^\033[0m\n" : "^\n", f);
    }
    fclose(src);
}

void diag_vreport(SrcLoc loc, const char *kind, const char *fmt, va_list ap)
{
    diag_print_prefix(stderr, loc, kind);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    diag_print_source(stderr, loc);
}

void diag_error(SrcLoc loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vreport(loc, "error", fmt, ap);
    va_end(ap);
}

void diag_warning(SrcLoc loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vreport(loc, "warning", fmt, ap);
    va_end(ap);
}

void diag_note(SrcLoc loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vreport(loc, "note", fmt, ap);
    va_end(ap);
}

void diag_count_error(void)
{
    diag_errors++;
    if (diag_max_errors > 0 && diag_errors >= diag_max_errors) {
        diag_print_prefix(stderr, (SrcLoc){ 0 }, "error");
        fprintf(stderr, "too many errors, stopping\n");
        exit(1);
    }
}

void diag_unwind(void)
{
    if (!diag_recover) {
        exit(1);
    }
    diag_count_error();
    longjmp(*diag_recover, 1);
}

void diag_abandon(void)
{
    if (!diag_recover) {
        exit(1);
    }
    longjmp(*diag_recover, 1);
}

void diag_fatal_v(const char *fmt, va_list ap)
{
    diag_vreport(diag_loc, "error", fmt, ap);
    diag_unwind();
}

void internal_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vreport(diag_loc, "internal compiler error", fmt, ap);
    va_end(ap);
    fprintf(stderr, "please report this bug\n");
    exit(2);
}
