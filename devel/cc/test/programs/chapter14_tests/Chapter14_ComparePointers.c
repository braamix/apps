int main(void) {
    int a = 0;
    int b;

    int *a_ptr = &a;
    int *a_ptr2 = &a;
    int *b_ptr = &b;

    if (a_ptr == b_ptr) {
        return 1;
    }

    if (a_ptr != a_ptr2) {
        return 2;
    }

    if (!(a_ptr == a_ptr2)) {
        return 3;
    }

    if (!(a_ptr != b_ptr)) {
        return 4;
    }

    *b_ptr = *a_ptr;
    if (a_ptr == b_ptr) {
        return 5;
    }
    b_ptr = a_ptr;
    if (b_ptr != a_ptr) {
        return 6;
    }

    return 0;
}
