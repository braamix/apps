int not(unsigned long ul) {
    return !ul;
}

int if_cond(unsigned u) {
    if (u) {
        return 1;
    }
    return 0;
}

int and(unsigned long ul, int i) {
    return ul && i;
}

int or(int i, unsigned u) {
    return i || u;
}

int main(void) {
    unsigned long ul = 123456789012345ul; // nonzero, < 2^48
    unsigned int u = 2147483648u; // 2^31
    unsigned long zero = 0l;
    if (not(ul)) {
        return 1;
    }
    if (!not(zero)) {
        return 2;
    }
    if(!if_cond(u)) {
        return 3;
    }
    if(if_cond(zero)) {
        return 4;
    }

    if (and(zero, 1)) {
        return 5;
    }

    if (!or(1, u)) {
        return 6;
    }

    return 0;
}
