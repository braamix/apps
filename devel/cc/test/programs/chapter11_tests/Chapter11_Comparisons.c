long l;
long l2;

int compare_constants(void) {
    return 8589934593l > 255l;
}

int compare_constants_2(void) {
    return 255l < 8589934593l;
}

int l_geq_2_39(void) {
    return (l >= 549755813888l);
}

int uint_max_leq_l(void) {
    return (4294967295l <= l);
}

int l_eq_l2(void) {
    return (l == l2);
}

int main(void) {
    if (!compare_constants()) {
        return 1;
    }
    if (!compare_constants_2()) {
        return 2;
    }
    l = -1099511627775l; // -(2^40 - 1)
    if (l_geq_2_39()) {
        return 3;
    }
    if (uint_max_leq_l()) {
        return 4;
    }
    l = 549755813888l; // 2^39
    if (!l_geq_2_39()) {
        return 5;
    }
    if (!uint_max_leq_l()) {
        return 6;
    }
    l2 = l;
    if (!l_eq_l2()) {
        return 7;
    }
    return 0;
}
