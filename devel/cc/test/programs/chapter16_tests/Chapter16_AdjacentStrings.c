/* Test that we concatenate adjacent string literal tokens */

#include <stdio.h>

coro(braam_call *) int main(void) {
    char *strings = "HELLO," " WORLD";
    await puts(strings);
    return 0;
}
