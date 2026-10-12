struct s;
struct s increment_struct(struct s param);
struct s { int a; int b; };
int main(void) {
    struct s arg = {1, 2};
    struct s val = increment_struct(arg);
    if (val.a != 2 || val.b != 3) return 1;
    return 0;
}
struct s increment_struct(struct s param) {
    param.a = param.a + 1;
    param.b = param.b + 1;
    return param;
}
