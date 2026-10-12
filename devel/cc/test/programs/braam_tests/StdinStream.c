#include <stdio.h>
coro(braam_call *) int main(int argc, char **argv)
{
    char s[9];
    printf("? ");
    for (int i = 0; i < 3 && await fgets(s, sizeof s, stdin); i++)
        printf("[%s] ", s);
    printf("\n");
    int c, n = 0;
    while ((c = await getchar()) != EOF) {
        putchar(c == '\n' ? '|' : c);
        n++;
    }
    printf("\n%d %s\n", n, feof(stdin) ? "eof" : "?");
    return 0;
}

