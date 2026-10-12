#include <stdio.h>
#include <stdlib.h>
#include <string.h>
coro(braam_call *) int main(int argc, char **argv)
{
    FILE *in = await fopen(argv[1], "rb");
    char *a = malloc(8000), *b = malloc(8000);
    size_t n = await fread(a, 1, 8000, in);
    await rewind(in);
    size_t m = 0;
    int c;
    while ((c = await fgetc(in)) != EOF)
        b[m++] = (char)c;
    await fclose(in);
    FILE *out = await fopen("braam-CopyFile.copy", "w");
    fwrite(a, 1, n, out);
    await fclose(out);
    printf("%d %d %s\n", (int)n, (int)m, n == m && !memcmp(a, b, n) ? "same" : "differ");
    return await remove(argv[1]);
}

