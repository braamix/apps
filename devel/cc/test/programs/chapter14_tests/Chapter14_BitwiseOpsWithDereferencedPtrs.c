int main(void) {
    unsigned int ui = 1048575u;          // 0xFFFFF, bits 0-19
    unsigned long ul = 2199022206976ul;  // bits 20-40
    unsigned int *ui_ptr = &ui;
    unsigned long *ul_ptr = &ul;

    if ((*ui_ptr & *ul_ptr) != 0) {
        return 1;
    }

    if ((*ui_ptr | *ul_ptr) != 2199023255551ul) { // 2^41 - 1
        return 2;
    }

    int i = -1;
    signed int *i_ptr = &i;
    if ((*i_ptr & ul) != *ul_ptr) {
        return 3;
    }

    if ((*i_ptr | *ul_ptr) != i) {
        return 4;
    }

    return 0;
}
