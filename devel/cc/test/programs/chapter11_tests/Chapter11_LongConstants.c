int main(void) {
    if (2147483647l + 2147483647l < 0l) {
        return 1;
    }
    if (19327352832 < 100l) { // 19327352832 == 2^34 + 2^31
        return 2;
    }
    return 0;
}
