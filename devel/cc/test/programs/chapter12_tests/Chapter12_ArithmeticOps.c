unsigned int ui_a;
unsigned int ui_b;

unsigned long ul_a;
unsigned long ul_b;

int addition(void) {
    return (ui_a + 2147483653u == 2147483663u);
}

int subtraction(void) {
    return (ul_a - ul_b == 281474976709000ul);
}

int multiplication(void) {
    return (ui_a * ui_b == 3221225472u);
}

int division(void) {
    return (ui_a / ui_b == 0);
}

int div_large(void) {
    return (ui_a / ui_b == 2);
}

int div_lit(void) {
    return (ul_a / 5ul == 219902325555ul);
}

int remaind(void) {
    return (ul_b % ul_a == 5ul);
}
int complement(void) {
    return (~ui_a == 0);
}

int main(void) {

    ui_a = 10u;
    if (!addition()) {
        return 1;
    }

    ul_a = 281474976710000ul;
    ul_b = 1000ul;
    if (!subtraction()) {
        return 2;
    }

    ui_a = 1073741824u;
    ui_b = 3u;
    if (!multiplication()) {
        return 3;
    }

    ui_a = 100u;
    ui_b = 4294967294u;

    if (!division()) {
        return 4;
    }

    ui_a = 4294967294u;
    ui_b = 2147483647u;
    if (!div_large()) {
        return 5;
    }

    ul_a = 1099511627775ul;
    if (!div_lit()) {
        return 6;
    }

    ul_a = 100ul;
    ul_b = 281474976710605ul;
    if (!remaind()) {
        return 7;
    }

    ui_a = 281474976710655U;
    if (!complement()) {
        return 8;
    }

    return 0;
}
