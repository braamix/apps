struct pair1 { int x; int y; };
struct pair2 { double x; char y; };
struct pair3 { int x; int *y; };
struct pair4 { void *x; double y[4]; };
struct x { int x; };
struct f { int f; };
int sharedmem(void) {
    struct pair1 p1 = {1, 2};
    struct pair2 p2 = {3.0, 4};
    if (p1.x != 1 || p2.x != 3.0) return 0;
    return 1;
}
int nestedmem(void) {
    struct pair3 p1 = {3, &(p1.x)};
    struct pair4 p2 = {&p1, {1.0, 2.0, 3.0, 4.0}};
    if (((struct pair3 *)p2.x)->x != 3) return 0;
    return 1;
}
int varname(void) {
    struct x x = {10};
    if (x.x != 10) return 0;
    return 1;
}
int f(void);
int funname(void) {
    struct f my_struct;
    my_struct.f = f();
    if (my_struct.f != 10) return 0;
    return 1;
}
int f(void) { return 10; }
int main(void) {
    if (!sharedmem()) return 1;
    if (!nestedmem()) return 2;
    if (!varname()) return 3;
    if (!funname()) return 4;
    return 0;
}
