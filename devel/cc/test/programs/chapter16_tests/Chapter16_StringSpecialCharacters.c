/* Test that we can handle special characters in string literals
 * that are not array initializers
 */

#include <stdio.h>
int strcmp(char *s1, char *s2);

coro(braam_call *) int main(void) {
    // string literal containing escape sequences
    char *escape_sequence = "\a\b";
    if (escape_sequence[0] != 7) {
        return 1;
    }

    if (escape_sequence[1] != 8) {
        return 2;
    }

    if (escape_sequence[2]) {// check for terminating null byte
        return 3;
    }

    // double quote
    char *with_double_quote = "HELLO\"WORLD";
    if (with_double_quote[5] != '"') {
        return 4;
    }
    await puts(with_double_quote);

    char *with_newline = "LINE\nBREAK!";
    if (with_newline[4] != 10) {
        return 6;
    }
    await puts(with_newline);

    // literal tab
    char *tab = "\t";
    if (strcmp(tab, "\t")) {
        return 7;
    }

    await puts("TESTING, 123.");

    return 0;
}
