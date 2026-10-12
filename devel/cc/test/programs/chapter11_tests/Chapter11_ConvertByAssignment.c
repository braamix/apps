int return_truncated_long(long l) {
    return l;
}

long return_extended_int(int i) {
    return i;
}

int truncate_on_assignment(long l, int expected) {
    int result = l;
    return result == expected;
}

int main(void) {
    long result = return_truncated_long(4294967298l);
    if (result != 2l) {
        return 1;
    }
    result = return_extended_int(-10);
    if (result != -10) {
        return 2;
    }
    int i = 4294967298l;
    if (i != 2) {
        return 3;
    }
    if (!truncate_on_assignment(17179869184l, 0)) {
        return 4;
    }
    return 0;
}
