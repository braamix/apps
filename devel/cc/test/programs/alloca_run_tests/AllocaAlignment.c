#include <alloca.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

static int program(void)
{
    static const int sizes[] = { 1, 3, 0, 5, 7, 2 };
    char *p[6];
    int aligned = 1, apart = 1;
    for (int i = 0; i < 6; i++) {
        p[i] = alloca(sizes[i]);
        if ((uintptr_t)p[i] % _Alignof(max_align_t) != 0)
            aligned = 0;
        for (int j = 0; j < sizes[i]; j++)
            p[i][j] = (char)i;
    }
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < sizes[i]; j++)
            if (p[i][j] != i)
                apart = 0;
    say("aligned %d apart %d\n", aligned, apart);
    return 0;
}

coro(braam_call *) int main(void)
{
    int status = program();
    await fputs(said, stdout);
    return status;
}
