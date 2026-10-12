int target(int arg) {
    int *optimized_away = &arg;
    return arg + 10;
}
int main(void) { return target(1) == 11 ? 0 : 1; }

