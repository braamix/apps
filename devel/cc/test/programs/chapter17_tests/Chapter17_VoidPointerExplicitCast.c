/* test explicit casts between void * and other pointer types,
 * and between void * and integer types
 */

int main(void) {
    static double dbuf[4];
    void *ptr = dbuf;
    double *double_ptr = (double *)ptr;
    double_ptr[2] = 10.0;
    if ((void *)double_ptr != ptr) {
        return 1;
    }
    double result = double_ptr[2];

    if (result != 10.0) {
        return 2;
    }

    long zero = 0;
    ptr = (void *) zero;
    if (ptr) {
        return 4;
    }
    zero = (long) ptr;
    if (zero) {
        return 5;
    }
    return 0;
}
