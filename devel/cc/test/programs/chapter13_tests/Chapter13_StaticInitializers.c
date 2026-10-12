double d1 = 2147483647;
double d2 = 4294967295u;
double d3 = 4611686018427389440l;
double d4 = 4611686018427389955l;
double d6 = 4611686018427389955ul;
double uninitialized;
static int i = 4.9;
int unsigned u = 42949.672923E5;
long l = 4611686018427389440.;
unsigned long ul = 18446744073709549568.;

int main(void) {
    if (d1 != 2147483647.) { return 1; }
    if (d2 != 4294967295.) { return 2; }
    if (d3 != 4611686018427389952.) { return 3; }
    if (d4 != d3) { return 4; }
    if (d6 != d3) { return 5; }
    if (uninitialized) { return 6; }
    if (i != 4) { return 7; }
    if (u != 4294967292u) { return 8; }
    if (l != 4611686018427389952l) { return 9; }
    if (ul != 18446744073709549568ul) { return 10; }
    return 0;
}
