long x = 5l;

int add_large(void) {
    x = x + 4294967290l; // this constant is 2^32 - 6
    return (x == 4294967295l);
}

int subtract_large(void) {
    x = x - 4294967290l;
    return (x == 5l);
}

int multiply_by_large(void) {
    x = x * 4294967290l;
    return (x == 21474836450l);
}

int main(void) {
    if (!add_large()) {
        return 1;
    }
    if (!subtract_large()) {
        return 2;
    }
    if (!multiply_by_large()) {
        return 3;
    }
    return 0;
}
