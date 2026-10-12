/* Constant-folding tests for conversions to double from chars and negative
 * ints; couldn't test these before because we need copy prop to fully evaluate
 * them.
 * */

double target_from_neg_int(void) {
    return (double)-2147483647;  // can convert exactly
}

double target_from_neg_long(void) {
    // original -4611686018427388416l is beyond BESM-6 range; use an in-range
    // value (exact as double).
    return (double)-100000000000l;
}

// test conversion from char to double
double target_from_char(void) {
    char c = 127;
    return (double)c;
}

// test conversion from signed char to double
double target_from_schar(void) {
    char c = -127;
    return (double)c;
}

// test conversion from uchar to double
double target_from_uchar(void) {
    unsigned char u = 255;
    return (double)u;
}

// if we initially assign char a value outside its range,
// make sure we truncate before converting to double
double target_from_truncated_char(void) {
    char c = -129;
    return (double)c;  // 127
}

// if we initially assign uchar a value outside its range,
// make sure we truncate before converting to double
double target_from_truncated_uchar(void) {
    unsigned char c = 1000;
    return (double)c;  // 232
}

double target_from_negated_int_zero(void) {
    // negating integer zero is just zero,
    // which will be converted to positive floating-point zero
    return -0;
}

int main(void) {
    if (target_from_neg_int() != -2147483647.) {
        return 1;
    }
    if (target_from_neg_long() != -100000000000.0) {
        return 2;
    }
    if (target_from_char() != 127) {
        return 3;
    }
    if (target_from_schar() != -127) {
        return 4;
    }
    if (target_from_uchar() != 255) {
        return 5;
    }
    if (target_from_truncated_char() != 127) {
        return 6;
    }
    if (target_from_truncated_uchar() != 232) {
        return 7;
    }
    double zero = target_from_negated_int_zero();
    if (zero != 0) {  // BESM-6 has no -0; negated zero is plain 0.0
        return 8;
    }
    return 0;  // success
}

