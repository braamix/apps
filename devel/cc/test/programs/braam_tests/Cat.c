#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

coro(braam_call *) int count(int fd, long *lines, long *words, long *chars)
{
    char buf[100];
    int in_word = 0;
    ssize_t n;
    while ((n = await read(fd, buf, sizeof buf)) > 0)
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            ++*chars;
            if (c == '\n')
                ++*lines;
            if (c == ' ' || c == '\n' || c == '\t')
                in_word = 0;
            else if (!in_word) {
                in_word = 1;
                ++*words;
            }
        }
    return n < 0 ? -1 : 0;
}

coro(braam_call *) int main(int argc, char **argv)
{
    int fd = argc > 1 ? await open(argv[1], O_RDONLY) : 0;
    if (fd < 0) {
        fprintf(stderr, "wc: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    long l = 0, w = 0, c = 0;
    if (await count(fd, &l, &w, &c) < 0)
        return 1;
    printf("%ld %ld %ld\n", l, w, c);
    if (fd)
        await close(fd);
    return 0;
}

