/* Test that we can use chars in the same expressions as other integers */

int add_chars(char c1, char c2) {
    return c1 + c2;
}

int divide_chars(unsigned char c1, unsigned char c2) {
    return c1 / c2;
}

int le(char c1, char c2) {
    return c1 <= c2;
}

int subscript_char(int *ptr, char idx){
    return ptr[idx];
}

int *sub_char_from_pointer(int *ptr, signed char idx) {
    return ptr - idx;
}

int and_char(signed char c1, int i) {
    return c1 && i;
}

int or_char(signed char c1, unsigned char c2) {
    return c1 || c2;
}

int test_for_loop_char(void) {
    int counter = 0;
    for (signed char s = 127; s > 0; s = s - 1) {
        counter = counter + 1;
    }
    return (counter == 127);
}

int main(void) {

    char c1 = 8;
    char c2 = 4;
    if (add_chars(c1, c2) != 12)  {
        return 1;
    }

    unsigned char uc1 = 250;
    unsigned char uc2 = 25;
    if (divide_chars(uc1, uc2) != 10) {
        return 2;
    }

    if (le(c1, c2)) {
        return 3;
    }

    if (!le(c2, c2)) {
        return 4;
    }

    int arr[4] = {11, 12, 13, 14};
    char idx = 2;
    if (subscript_char(arr, idx) != 13) {
        return 5;
    }

    signed char offset = 1;
    if (sub_char_from_pointer(arr + 1, offset) != arr) {
        return 6;
    }

    signed char zero = 0;
    if (zero) {
        return 7;
    }

    if (and_char(zero, 12)) {
        return 8;
    }

    uc2 = 0;
    if (or_char(zero, uc2)) {
        return 9;
    }

    if (!test_for_loop_char()) {
        return 10;
    }

    return 0;
}
