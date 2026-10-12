static int long a;
int static long a;
long static a;

int my_function(long a, long int b, int long c);
int my_function(long int x, int long y, long z) {
    return x + y + z;
}

int main(void) {
    long x = 1l;
    long int y = 2l;
    int long z = 3l;
    extern long a;
    a = 4;
   int sum = 0;
    for (long i = 549755813888l; i > 0; i = i / 2) {
        sum = sum + 1;
    }
    if (x != 1) {
        return 1;
    }
    if (y != 2) {
        return 2;
    }
    if (a != 4) {
        return 3;
    }
    if (my_function(x,  y, z) != 6) {
        return 4;
    }
    if (sum != 40) {
        return 5;
    }
    return 0;
}
