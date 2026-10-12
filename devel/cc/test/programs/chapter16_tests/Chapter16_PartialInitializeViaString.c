/* Test that when we initialize an array from a string literal,
 * we zero out elements that aren'T EXPLICITLY INITIALIZED.
 * */

static char static_arr[5] = "HI";
int test_static(void) {
    return (static_arr[0] == 'H' && static_arr[1] == 'I' &&
            !(static_arr[2] || static_arr[3] || static_arr[4]));
}

static signed char nested_static_arr[3][4] = {"", "BC"};
int test_static_nested(void) {
    for (int i = 0; i < 3; i = i + 1)
        for (int j = 0; j < 4; j = j + 1) {
            signed char c = nested_static_arr[i][j];
            signed char expected = 0;
            if (i == 1 && j == 0) {
                expected = 'B';
            } else if (i == 1 && j == 1) {
                expected = 'C';
            }
            if (c != expected) {
                return 0;
            }
        }
    return 1;
}

int test_automatic(void) {
    unsigned char aut[4] = "AB";
    return (aut[0] == 'A' && aut[1] == 'B' && !(aut[2] || aut[3]));
}

int test_automatic_nested(void) {
    signed char nested_auto[2][2][4] = {{"FOO"}, {"X", "YZ"}};
    for (int i = 0; i < 2; i = i + 1) {
        for (int j = 0; j < 2; j = j + 1) {
            for (int k = 0; k < 4; k = k + 1) {
                signed char c = nested_auto[i][j][k];
                signed char expected = 0;
                if (i == 0 && j == 0) {
                    if (k == 0) {
                        expected = 'F';
                    } else if (k == 1 || k == 2) {
                        expected = 'O';
                    }
                } else if (i == 1 && j == 0 && k == 0) {
                    expected = 'X';
                } else if (i == 1 && j == 1 && k == 0) {
                    expected = 'Y';
                } else if (i == 1 && j == 1 && k == 1) {
                    expected = 'Z';
                }
                if (c != expected) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

int main(void) {
    if (!test_static()) return 1;
    if (!test_static_nested()) return 2;
    if (!test_automatic()) return 3;
    if (!test_automatic_nested()) return 4;
    return 0;
}
