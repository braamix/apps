int glob;
int test_movsx_src(int i) {
    check_one_int(i - 10, -5);
    long l = 0;
    l = (long)i;
    check_one_long(l, 5l);
    return 0;
}
signed char glob_char = 10;
int test_movsx_dst(void) {
    unsigned long a = id(-1);
    unsigned long b = id(2);
    signed char neg_char = -glob_char;
    signed char not_char = ~glob_char;
    int c = (int)glob_char;
    long d = id(4);
    unsigned int e = (unsigned int)neg_char;
    long f = (long) not_char;
    check_one_ulong(a, 18446744073709551615ul);
    check_one_ulong(b, 2ul);
    check_one_int(c, 10);
    check_one_long(d, 4l);
    check_one_uint(e, -10);
    check_one_long(f, -11);
    return 0;
}
unsigned int glob_uint;
int test_movzx_src(unsigned int u) {
    check_one_uint(u + 10u, 30u);
    long l = (long)u;
    check_one_long(l, 20l);
    return 0;
}
int test_movzx_dst(void) {
    long a = (long)unsigned_id(2000u);
    unsigned long b = (unsigned long)unsigned_id(1000u);
    unsigned long c = (unsigned long)unsigned_id(255u);
    long d = (long)unsigned_id(4294967295U);
    long e = (long)unsigned_id(2147483650u);
    unsigned long f = (unsigned long)unsigned_id(80u);
    check_one_long(a, 2000l);
    check_one_ulong(b, 1000ul);
    check_one_ulong(c, 255ul);
    check_one_long(d, 4294967295l);
    check_one_long(e, 2147483650l);
    check_one_ulong(f, 80ul);
    return 0;
}
int test_movzbq_src(unsigned char c) {
    unsigned char d = c + 1;
    check_one_uchar(d, 13);
    long l = (long)c;
    check_one_long(l, 12);
    return 0;
}
int test_movzb_dst(void) {
    int a = (int)uchar_id(200);
    unsigned int b = (unsigned int)uchar_id(100);
    unsigned long c = (unsigned long)uchar_id(255);
    long d = (long)uchar_id(77);
    long e = (long)uchar_id(125);
    unsigned long f = (unsigned long)uchar_id(80);
    check_one_int(a, 200);
    check_one_uint(b, 100u);
    check_one_ulong(c, 255ul);
    check_one_long(d, 77l);
    check_one_long(e, 125l);
    check_one_ulong(f, 80ul);
    return 0;
}
int test_cvtsi2sd_src(int i) {
    check_one_int(i + 10, 16);
    double d = (double)i;
    check_one_double(d, 6.0);
    return 0;
}
int global_int = 5000;
long global_long = 5005;
int test_cvtsi2sd_dst(void) {
    double d0 = (double)global_int;
    double d1 = (double)(global_long - 4l);
    double d2 = (double)(global_int + 2);
    double d3 = (double)(global_long - 2l);
    double d4 = (double)(global_int + 4);
    double d5 = (double)(global_int + 5);
    double d6 = (double)(global_int + 6);
    double d7 = (double)(global_int + 7);
    double d8 = (double)(global_int + 8);
    double d9 = (double)(global_int + 9);
    double d10 = (double)(global_int + 10);
    double d11 = (double)(global_int + 11);
    double d12 = (double)(global_int + 12);
    double d13 = (double)(global_int + 13);
    double d14 = (double)(global_int + 14);
    global_long = (long)d14;
    check_14_doubles(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13,
                     5000);
    check_one_int(global_int, 5000);
    check_one_long(global_long, 5014l);
    return 0;
}
double glob_dbl;
int test_cvttsd2si_src(double d) {
    glob_dbl = d + 10.0;
    int i = (int)d;
    check_one_int(i, 7);
    check_one_double(glob_dbl, 17.0);
    return 0;
}
int test_cvttsd2si_dst(void) {
    int a = (int)dbl_id(-200.0);
    long b = (long)dbl_id(-300.0);
    int c = (int)dbl_id(-400.0);
    long d = (long)dbl_id(-500.0);
    int e = (int)dbl_id(-600.0);
    long f = (long)dbl_id(-700.0);
    check_one_int(a, -200);
    check_one_long(b, -300l);
    check_one_int(c, -400);
    check_one_long(d, -500l);
    check_one_int(e, -600);
    check_one_long(f, -700l);
    return 0;
}
int main(void) {
    test_movsx_src(5);
    test_movsx_dst();
    test_movzx_src(20u);
    test_movzx_dst();
    test_movzbq_src(12);
    test_movzb_dst();
    test_cvtsi2sd_src(6);
    test_cvtsi2sd_dst();
    test_cvttsd2si_src(7.0);
    test_cvttsd2si_dst();
    return 0;
}

