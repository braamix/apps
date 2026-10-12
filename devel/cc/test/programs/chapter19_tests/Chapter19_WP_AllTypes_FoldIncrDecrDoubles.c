/* Make sure we can constant fold ++/-- operations on doubles.
 * The book's mantissa-edge case (2^53) has no BESM-6 analogue: constant folding
 * is done in host double precision, so the precision loss the book checks for
 * only happens at runtime, not in the fold.  We keep the exactly-representable
 * cases, which fold identically on host and BESM-6. */

int target(void) {
    double e1 = 10.0;
    double e2 = --e1;  // 9; e1 -> 9
    double e3 = e1--;  // 9; e1 -> 8
    double e4 = e1++;  // 8; e1 -> 9
    double e5 = ++e1;  // 10; e1 -> 10

    if (!(e1 == 10. && e2 == 9. && e3 == 9. && e4 == 8. && e5 == 10.)) {
        return 1; // fail
    }

    return 0; // success
}

int main(void) {
    return target();
}

