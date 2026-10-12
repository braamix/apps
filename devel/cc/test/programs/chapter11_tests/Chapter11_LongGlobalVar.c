extern long int l;
long return_l(void);
int return_l_as_int(void);

int main(void) {
    if (return_l() != 8589934592l)
        return 1;
    if (return_l_as_int() != 0)
        return 2;
    l = l - 10l;
    if (return_l() != 8589934582l)
        return 3;
    if (return_l_as_int() != -10)
        return 4;
    return 0;
}

long int l = 8589934592l; // 2^33

long return_l(void) {
    return l;
}

int return_l_as_int(void) {
    return l;
}
