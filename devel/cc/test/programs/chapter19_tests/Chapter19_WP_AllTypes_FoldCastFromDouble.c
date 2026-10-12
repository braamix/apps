/* Constant-folding tests for conversions from negative doubles to integer
 * types; couldn't test these before because we need copy prop to fully evaluate
 * them.
 * */

char target_to_char(void) {
    return (char)-126.5;
}

int target_to_int(void) {
    return (int)-5.9;
}

long target_to_long(void) {
    // original -9223372036854774783.1 is beyond BESM-6 range; use an in-range
    // value (truncation toward zero).
    return (long)-100000000000.5;
}

int main(void) {
    if (target_to_char() != -126) {
        return 1;
    }
    if (target_to_int() != -5) {
        return 2;
    }
    if (target_to_long() != -100000000000l) {
        return 3;
    }
    return 0;
}

