/* Test chaining multiple explicit casts together*/

// start with a global variable so we can't optimize away casts in Part III
unsigned int ui = 4294967200u;  // 2^32 - 96

int main(void) {
    ui = (unsigned int)(unsigned char)ui;
    if (ui != 160) {
        return 1;
    }

    int i = (int)(signed char)ui;
    if (i != -96) {
        return 2;
    }

    return 0;
}
