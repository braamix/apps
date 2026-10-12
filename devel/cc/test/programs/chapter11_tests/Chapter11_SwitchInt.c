int switch_on_int(int i) {
    switch(i) {
        case 5:
            return 0;
        case 8589934592l: // 2^33
            return 1;
        case 34359738367: // ~3.4e10
            return 2;
        default:
            return 3;
    }
}

int main(void) {
    if (switch_on_int(5) != 0)
        return 1;
    if (switch_on_int(0) != 1)
        return 2;
    if (switch_on_int(-1) != 2)
        return 3;
    if (switch_on_int(17179869184) != 1)
        return 4;
    return 0;
}
