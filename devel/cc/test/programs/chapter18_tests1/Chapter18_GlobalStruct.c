struct s { int i; char arr[2]; double d; };
struct outer { char c; struct s inner; };
extern struct s global;
extern struct outer global_outer;
void update_struct(void);
void update_outer_struct(void);
struct s global = {1, {2, 3}, 4.0};
struct outer global_outer = {5, {6, {7, 8}, 9.0}};
int main(void) {
    update_struct();
    if (global.arr[1] != 4) return 1;
    if (global.d != 5.0) return 2;
    update_outer_struct();
    if (global_outer.c != 5) return 3;
    if (global_outer.inner.i || global_outer.inner.d) return 4;
    if (global_outer.inner.arr[0] != 11 || global_outer.inner.arr[1] != 12) return 5;
    return 0;
}
void update_struct(void) {
    global.arr[1] = global.arr[0] * 2;
    global.d = 5.0;
}
void update_outer_struct(void) {
    struct s inner = {0, {11, 12}, 0};
    global_outer.inner = inner;
}

