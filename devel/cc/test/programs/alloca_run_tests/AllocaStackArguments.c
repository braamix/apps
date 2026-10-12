#include <alloca.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// On Braam printf is a coroutine, and a function that calls alloca cannot be one:
// what it prints is kept, and main prints it.
static char said[4096];
static int said_len;

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    said_len += vsnprintf(said + said_len, sizeof said - said_len, fmt, ap);
    va_end(ap);
}

static int sum12(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j,
                 int k, int l)
{
    return a + b + c + d + e + f + g + h + i + j + k + l;
}

static int program(void)
{
    char *p = alloca(13);
    strcpy(p, "abcdefghijkl");
    int s = sum12(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
    char *q = alloca(4);
    strcpy(q, "xyz");
    say("%d %s\n", sum12(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) == s ? s : -1, p);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = program();
    await fputs(said, stdout);
    return status;
}
