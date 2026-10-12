int glob = 1;
int target(int a, int b, int c, int d, int e, int f) {
    int g = a + f;
    int h = b * d;
    int i = c * c;
    int j = d + f;
    int k = e + f;
    int l = f * b;
    int m = (a + b + c + d + e + f) - 7;
    int n = g + h;
    int o = i + 7;
    int p = j * 2 - 3;
    int q = k + g;
    check_12_ints(g, h, i, j, k, l, 13, m, n, o, p, q, 7);
    return 0;
}
int main(void) { return target(1, 2, 3, 4, 5, 6); }

