int main(void) {
    unsigned int ui = 16777215u; // 2^24 - 1, low bits set
    unsigned long ul = 1099511627776ul; // 2^40, a single high bit

    if ((ui & ul) != 0)
        return 1;

    if ((ui | ul) != 1099528404991ul) // 2^40 + 2^24 - 1
        return 2;

    signed int i = -1;
    if ((i & ul) != ul)
        return 3;

    if ((i | ul) != i)
        return 4;

    return 0;
}
