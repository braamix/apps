double glob = 3.0;

int main(void) {
    long l = -1l;
    int i = -1;
    int j = (int) glob;
    int k = 20;
    if (l != -1l) { return 1; }
    if (i != -1) { return 2; }
    if (j != 3) { return 3; }
    if (k != 20) { return 4; }
    return 0;
}
