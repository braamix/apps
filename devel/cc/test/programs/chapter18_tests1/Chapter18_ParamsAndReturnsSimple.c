struct pair { int x; char y; };
struct pair2 { double d; long l; };
struct pair2 double_members(struct pair p) {
    struct pair2 retval = {p.x * 2, p.y * 2};
    return retval;
}
int main(void) {
    struct pair arg = {1, 4};
    struct pair2 result = double_members(arg);
    if (result.d != 2.0 || result.l != 8) return 1;
    return 0;
}
