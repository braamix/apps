/* Test that we correctly get the size of a range of expressions */

int main(void) {
    double d;

    if (sizeof d != 8) {
        return 2;
    }

    unsigned char c;

    if (sizeof c != 1) {
        return 3;
    }

    static char sbuf[100];
    void *buffer = sbuf;

    if (sizeof(buffer) != 8) {
        return 4;
    }

    if (sizeof ((int)d) != 4) {
        return 5;
    }

    if (sizeof (d ? c : 10l) != 8) {
        return 6;
    }

    if (sizeof (c = 10.0) != 1) {
        return 7;
    }

    return 0;
}
