/* Test constant folding with negative numbers (including double and long);
 * we couldn't test this in the constant-folding stage because it requires
 * copy propagation.  Adapted for BESM-6: long is 41-bit (so the long values
 * stay within +/-2^40), the huge-magnitude double becomes -2^54 (where +1 is
 * still lost in the host-precision fold), the subnormal-cancellation case has
 * no BESM-6 analogue and is replaced by a normal subtraction, and function
 * names are kept distinct within 8 characters.
 * */

/* long tests */

long t_rem(void) {
    return -8589934585l % 4294967290l;
}

long t_lsub(void) {
    return -4294967290l - 90l;
}

long t_ldiv(void) {
    return (-4294967290l / 128l);
}

long t_lcompl(void) {
    return ~-1099511627775l;  // ~(-(2^40-1)) == 2^40-2
}

/* double tests */
double t_dadd(void) {
    // -2^54 is large enough that adding one doesn't change it in the
    // host-precision constant fold, and is representable on BESM-6.
    return -18014398509481984.0 + 1.;
}

double t_dsub(void) {
    // ordinary subtraction of two negative values (the book's subnormal
    // cancellation has no analogue on BESM-6's float format)
    return -5000.0 - -1234.5;
}

double t_ddiv(void) {
    return -1100.5 / 5000.;
}

int main(void) {
    // long tests
    if (t_rem() != -5l) {
        return 1;  // fail
    }
    if (t_lsub() != -4294967380l) {
        return 2;  // fail
    }
    if (t_ldiv() != -33554431l) {
        return 3;  // fail
    }
    if (t_lcompl() != 1099511627774l) {
        return 4;  // fail
    }
    // double tests
    if (t_dadd() != -18014398509481984.0) {
        return 5;  // fail
    }
    if (t_dsub() != -3765.5) {
        return 6;  // fail
    }
    if (t_ddiv() != -0.2201) {
        return 7;  // fail
    }
    return 0;
}

