long a;
long b;

int addition(void) {
    return (a + b == 4294967295l);
}

int subtraction(void) {
    return (a - b == -4294967380l);
}

int multiplication(void) {
    return (a * 4l == 17179869160l);
}

int division(void) {
    b = a / 128l;
    return (b == 33554431l);
}

int remaind(void) {
    b = -a % 4294967290l;
    return (b == -5l);
}

int complement(void) {
    return (~a == -1099511627775l);
}

int main(void) {
    a = 4294967290l; // 2^32 - 6
    b = 5l;
    if (!addition()) {
        return 1;
    }
    a = -4294967290l;
    b = 90l;
    if (!subtraction()) {
        return 2;
    }
    a = 4294967290l;
    if (!multiplication()) {
        return 3;
    }
    a = 4294967290l;
    if (!division()) {
        return 4;
    }
    a = 8589934585l; // 2^33 - 7
    if (!remaind()) {
        return 5;
    }
    a = 1099511627774l; // 2^40 - 2
    if (!complement()) {
        return 6;
    }
    return 0;
}
