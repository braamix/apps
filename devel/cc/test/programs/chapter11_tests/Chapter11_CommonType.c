long l;
int i;

int addition(void) {
    long result = i + l;
    return (result == 2147483663l);
}

int division(void) {
    int int_result = l / i;
    return (int_result == 214748364);
}

int comparison(void) {
    return (i <= l);
}

int conditional(void) {
    long result = 1 ? l : i;
    return (result == 8589934592l);
}

int main(void) {
    l = 2147483653;
    i = 10;
    if (!addition()) {
        return 1;
    }
    l = 2147483649l;
    if (!division()) {
        return 2;
    }
    i = -100;
    l = 2147483648; // 2^31
    if (!comparison()) {
        return 3;
    }
    l = 8589934592l; // 2^33
    i = 10;
    if (!conditional()) {
        return 4;
    }
    return 0;
}
