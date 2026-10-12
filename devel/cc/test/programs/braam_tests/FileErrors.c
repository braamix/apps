#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
coro(braam_call *) int main(int argc, char **argv)
{
    struct stat st;
    FILE *f = await fopen("braam-FileErrors.none", "r");
    printf("fopen %d %s\n", f == NULL, strerror(errno));
    perror("braam-FileErrors.none");
    f = await fopen("x", "q");
    printf("mode %d %d\n", f == NULL, errno == EINVAL ? 22 : errno);
    int s = await stat("braam-FileErrors.none", &st);
    printf("stat %d %d\n", s, errno);
    printf("fstat %d\n", await fstat(1, &st));
    return 0;
}

