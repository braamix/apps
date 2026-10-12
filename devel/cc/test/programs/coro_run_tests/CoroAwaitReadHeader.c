#include <coro.h>
#include <stdio.h>
#include <string.h>

typedef struct { int fd; void *buf; size_t len; int out; } io_req;
struct header { char a[4], b[4]; };

coro(io_req *) int read_exact(int fd, char *p, size_t n)
{
    size_t got = 0;
    while (got < n) {
        io_req r = { .fd = fd, .buf = p + got, .len = n - got };
        if ((yield &r) == CO_CANCEL)
            return -1;
        if (r.out <= 0)
            return got;
        got += r.out;
    }
    return got;
}

coro(io_req *) int read_header(int fd, struct header *h)
{
    int n = await read_exact(fd, (char *)h, sizeof *h);
    return n == sizeof *h ? 0 : -1;
}

static const char *input;

static void serve(io_req *r)
{
    printf("req %d %d\n", r->fd, (int)r->len);
    size_t n = strlen(input);
    if (n > 3)
        n = 3;
    if (n > r->len)
        n = r->len;
    memcpy(r->buf, input, n);
    input += n;
    r->out = n;
}

static int run(const char *text, struct header *h)
{
    static _Alignas(16) char storage[512];
    input = text;
    co_frame(io_req *, int) *t = co_init(storage, sizeof storage, read_header, 0, h);
    while (co_resume(t) == CO_SUSPENDED)
        serve(co_value(t));
    return co_result(t);
}

int main(void)
{
    struct header h;
    int st = run("1234abcd", &h);
    printf("header %d: %.4s %.4s\n", st, h.a, h.b);
    printf("short %d\n", run("12345", &h));
    return 0;
}

