int accept_unsigned(unsigned int a, unsigned int b, unsigned long c, unsigned long d,
                 unsigned int e, unsigned int f, unsigned long g, unsigned int h,
                 unsigned long i);

int main(void) {
    return accept_unsigned(1, 4294967295u, 281474976710655u, 140737488355328u, 2147483648u, 0, 123456, 2147487744u, 200000000000000u);
}

int accept_unsigned(unsigned int a, unsigned int b, unsigned long c, unsigned long d,
                 unsigned int e, unsigned int f, unsigned long g,
                 unsigned int h, unsigned long i) {
    if (a != 1u) {
        return 1;
    }
    if (b != 4294967295u) {
        return 2;
    }
    if (c != 281474976710655u) {
        return 3;
    }
    if (d != 140737488355328u) {
        return 4;
    }
    if (e != 2147483648u) {
        return 5;
    }
    if (f != 0u) {
        return 8;
    }
    if (g != 123456u) {
        return 9;
    }
    if (h != 2147487744u) {
        return 10;
    }
    if (i != 200000000000000u) {
        return 11;
    }
    return 0;
}
