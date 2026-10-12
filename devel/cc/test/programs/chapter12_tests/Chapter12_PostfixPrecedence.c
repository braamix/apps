int main(void) {
    unsigned int ui = 4294967295U; // 2^32 - 1

    if (((unsigned long)ui++) != 4294967295U) {
        return 1; // fail
    }
    if (ui) {
        return 2; // fail - ui should be 0 after update
    }
    return 0; // success
}
