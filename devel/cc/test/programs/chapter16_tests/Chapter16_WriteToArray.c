// Test writing to a char array

int puts(char *s);

int main(void) {
    // start with a flat array
    char flat_arr[4] = "ABC";
    puts(flat_arr);

    // update it
    flat_arr[2] = 'X';
    puts(flat_arr);

    // similar test with nested array
    char nested_array[2][6] = {"HELLO", "WORLD"};
    puts(nested_array[0]);
    puts(nested_array[1]);

    nested_array[0][0] = 'J';
    puts(nested_array[0]);

    return 0;
}
