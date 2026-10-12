unsigned long a = 8589934580ul; // 2^33 - 12

int main(void) {

    unsigned long b = (unsigned long) (unsigned int) a;

    if (b != 4294967284ul)
        return 1;

    b = (unsigned long) (signed int) a;
    if (b != 18446744073709551604ul)
        return 2;

    return 0;
}
