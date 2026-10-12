/* Test explicit conversions to and from character types */

unsigned char c2uc(char c) { return (unsigned char)c; }
signed char c2sc(char c) { return (signed char)c; }
signed char uc2c(unsigned char u) { return (signed char)u; }
signed char uc2sc(unsigned char u) { return (signed char)u; }
unsigned char sc2uc(signed char u) { return (unsigned char)u; }
int c2i(char c) { return (int)c; }
unsigned long c2ul(char c) { return (unsigned long)c; }
long sc2l(signed char s) { return (long)s; }
unsigned int sc2ui(signed char s) { return (unsigned int)s; }
double sc2d(signed char s) { return (double)s; }
int uc2i(unsigned char u) { return (int)u; }
unsigned int uc2ui(unsigned char u) { return (unsigned int)u; }
long uc2l(unsigned char u) { return (long)u; }
unsigned long uc2ul(unsigned char u) { return (unsigned long)u; }
double uc2d(unsigned char u) { return (double)u; }
char i2c(int i) { return (char)i; }
char ui2c(unsigned int u) { return (char)u; }
signed char d2c(double d) { return (signed char)d; }
signed char ul2sc(unsigned long l) { return (signed char)l; }
unsigned char i2uc(int i) { return (unsigned char)i; }
unsigned char ui2uc(unsigned int ui) { return (unsigned char)ui; }
unsigned char l2uc(long l) { return (unsigned char)l; }
unsigned char ul2uc(unsigned long ul) { return (unsigned char)ul; }
unsigned char d2uc(double d) { return (unsigned char)d; }
signed char l2sc(long l) { return (signed char)l; }

int main(void) {
    char c = 127;
    if (c2uc(c) != 127) return 1;
    if (c2i(c) != 127) return 2;
    if (c2ul(c) != 127) return 3;

    signed char sc = -10;
    if (sc2uc(sc) != 246) return 4;
    if (sc2l(sc) != -10) return 5;
    if (sc2ui(sc) != 4294967286u) return 6;
    if (sc2d(sc) != -10.0) return 7;

    unsigned char uc = 250;
    if (uc2i(uc) != 250) return 8;
    if (uc2l(uc) != 250) return 9;
    if (uc2ui(uc) != 250) return 10;
    if (uc2ul(uc) != 250) return 11;
    if (uc2d(uc) != 250.0) return 12;
    if (uc2sc(uc) != -6) return 13;
    if (uc2c(uc) != -6) return 14;

    c = (char)-128;
    if (i2c(128) != c) return 15;
    c = (char)-6;
    if (ui2c(2147483898u) != c) return 16;
    if (d2c(-2.6) != -2) return 17;

    if (l2sc(17592186044416l)) return 18;
    sc = (signed char)-126;
    if (ul2sc(9224497936761618562ul) != sc) return 19;

    uc = (unsigned char)200;
    if (i2uc(-1234488) != uc) return 20;
    if (ui2uc(4293732808) != uc) return 21;
    if (l2uc(-36283884951096l) != uc) return 22;
    if (ul2uc(9224497936761618632ul) != uc) return 23;
    if (d2uc(200.99) != uc) return 24;

    static long *null_ptr;
    char zero = (char)null_ptr;
    if (zero) return 25;

    c = 32;
    int *i = (int *)c;
    if ((char)i != c) return 26;

    if ((char)300 != (char)44) return 27;

    return 0;
}
