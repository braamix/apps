union u { long l; double d; };
int main(void) {
    union u x = {1000};
    (void)x;
    return 0;
}
