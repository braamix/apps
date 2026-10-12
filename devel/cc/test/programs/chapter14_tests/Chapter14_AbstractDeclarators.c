int main(void) {

    long int unsigned *x = 0;

    if (x != (unsigned long (*)) 0)
        return 1;

    if (x != (long unsigned int ((((*))))) 0)
        return 2;

    double ***y = 0;

    if (y != (double *(**)) 0)
        return 3;

    if (y != (double (***)) 0)
        return 4;

    if ((double (*(*(*)))) 0 != y)
        return 5;

    return 0;
}
