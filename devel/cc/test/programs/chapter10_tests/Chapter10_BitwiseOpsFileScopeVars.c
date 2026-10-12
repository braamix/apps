int x = 1;
int y = 0;

int main(void) {
    y = -1;
    x = (x << 1) | 1;
    if (x != 3) {
        return 1;
    }
    y = ((y & -5) ^ 12) >> 2;
    if (y != -3) {
        return 2;
    }
    return 0;
}
