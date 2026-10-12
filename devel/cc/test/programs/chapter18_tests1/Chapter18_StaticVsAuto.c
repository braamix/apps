struct s { int a; int b; };
int main(void) {
    for (int i = 0; i < 10; i = i + 1) {
        struct s autom = {1, 2};
        static struct s stat = {1, 2};
        autom.a = autom.a + 1;
        autom.b = autom.b + 1;
        stat.a = stat.a + 1;
        stat.b = stat.b + 1;
        if (i == 9) {
            if (stat.a != 11 || stat.b != 12) return 1;
            if (autom.a != 2 || autom.b != 3) return 2;
        }
    }
    return 0;
}
