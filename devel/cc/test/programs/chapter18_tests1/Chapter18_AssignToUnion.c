struct s { int a; int b; };
union u { struct s str; long l; double arr[3]; };
int main(void) {
    union u x = { {1, 2} };
    union u y = { {0, 0} };
    y = x;
    if (y.str.a != 1) return 1;
    if (y.str.b != 2) return 2;
    x.arr[0] = -20.;
    x.arr[1] = -30.;
    x.arr[2] = -40.;
    y = x;
    if (y.arr[0] != -20.) return 3;
    if (y.arr[1] != -30.) return 4;
    if (y.arr[2] != -40.) return 5;
    return 0;
}
