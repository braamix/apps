// Increment and decrement lvalues of character type
int main(void) {
    static signed char chars[5] = {123, 124, 125, 126, 127}; // plain char unsigned on BESM-6
    if (chars[0]++ != 123) {
        return 1;  // fail
    }

    if (chars[1]-- != 124) {
        return 2;  // fail
    }

    if (++chars[2] != 126) {
        return 3;  // fail
    }

    if (--chars[3] != 125) {
        return 4;  // fail
    }

    if (++chars[4] != -128) {
        return 5;  // fail
    }

    if (chars[0] != 124) {
        return 6;  // fail
    }

    if (chars[1] != 123) {
        return 7;  // fail
    }
    if (chars[2] != 126) {
        return 8;  // fail
    }
    if (chars[3] != 125) {
        return 9;  // fail
    }
    if (chars[4] != -128) {
        return 10;  // fail
    }

    signed char c = -128;
    c--;
    if (c != 127) {
        return 11;  // fail
    }

    return 0;  // success
}
