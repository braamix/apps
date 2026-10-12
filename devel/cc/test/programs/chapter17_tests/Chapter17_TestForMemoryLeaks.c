#include <stdlib.h>

long sum = 0;
void lots_of_args(int a, int b, int c, int d, int e, int f, int g, int h, int i,
                  int j, int k, int l, int m, int n, int o) {
    if (a != 1) { exit(1); }
    if (b != 2) { exit(2); }
    if (c != 3) { exit(3); }
    if (d != 4) { exit(4); }
    if (e != 5) { exit(5); }
    if (f != 6) { exit(6); }
    if (g != 7) { exit(7); }
    if (h != 8) { exit(8); }
    if (i != 9) { exit(9); }
    if (j != 10) { exit(10); }
    if (k != 11) { exit(11); }
    if (l != 12) { exit(12); }
    if (m != 13) { exit(13); }
    if (n != 14) { exit(14); }
    sum = sum + o;
    return;
}

int main(void) {
    for (int i = 0; i < 100; i = i + 1) {
        lots_of_args(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, i);
    }
    if (sum != 4950) {
        return 15;
    }
    return 0;
}
