//
// A whole file into memory, which is how a pass reads its input.
//
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "host.h"

coro(braam_call *) char *read_file(const char *path, size_t *len)
{
    int fd = await open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    size_t cap = 4096, n = 0;
    char *text = malloc(cap + 1);
    for (;;) {
        if (!text) {
            await close(fd);
            return NULL;
        }
        ssize_t got = await read(fd, text + n, cap - n);
        if (got < 0) {
            free(text);
            await close(fd);
            return NULL;
        }
        if (got == 0)
            break;
        n += got;
        if (n == cap) {
            cap *= 2;
            char *more = realloc(text, cap + 1);
            if (!more)
                free(text);
            text = more;
        }
    }
    await close(fd);
    text[n] = 0;
    *len    = n;
    return text;
}
