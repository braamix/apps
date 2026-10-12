unsigned int ui = 4294967200u; // 2^32 - 96

int main(void) {

    if ((long) (signed) ui != -96l)
        return 1;

    if ((unsigned long) (signed) ui != 18446744073709551520ul) // 2^64 - 96
        return 2;

    return 0;
}
