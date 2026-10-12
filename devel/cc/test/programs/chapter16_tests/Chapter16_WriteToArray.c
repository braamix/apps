// Test writing to a char array

#include <stdio.h>

coro(braam_call *) int main(void) {
    // start with a flat array
    char flat_arr[4] = "ABC";
    await puts(flat_arr);

    // update it
    flat_arr[2] = 'X';
    await puts(flat_arr);

    // similar test with nested array
    char nested_array[2][6] = {"HELLO", "WORLD"};
    await puts(nested_array[0]);
    await puts(nested_array[1]);

    nested_array[0][0] = 'J';
    await puts(nested_array[0]);

    return 0;
}
