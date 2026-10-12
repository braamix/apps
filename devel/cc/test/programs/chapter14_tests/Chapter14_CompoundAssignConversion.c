int main(void) {
    double d = 5.0;
    double *d_ptr = &d;
    *d_ptr *= 1000u;
    if (d != 5000.0) {
        return 1;
    }
    int i = -50;
    int *i_ptr = &i;
    *i_ptr %= 4294967200U;
    if (*i_ptr != 
