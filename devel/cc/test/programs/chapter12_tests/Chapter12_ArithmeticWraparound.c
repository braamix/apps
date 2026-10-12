unsigned int ui_a;
unsigned int ui_b;

unsigned long ul_a;
unsigned long ul_b;

int addition(void) {
    return ui_a + ui_b == 0u;
}

int subtraction(void) {
    return (ul_a - ul_b == 18446744073709551606ul);
}

int neg(void) {
    return -ul_a == 18446744073709551615UL;
}

int main(void) {
    ui_a = 4294967293u;
    ui_b = 3u;
    if (!addition()) {
        return 1;
    }

    ul_a = 10ul;
    ul_b = 20ul;
    if (!subtraction()) {
        return 2;
    }

    ul_a = 1ul;
    if (!neg()) {
        return 3;
    }

    return 0;
}
