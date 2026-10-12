struct pair { int a; int b; };
int main(void) {
    struct pair x = {1, 2};
    if (x.a != 1 || x.b != 2) return 1;
    struct pair *x_ptr = &x;
    if (x_ptr->a != 1 || x_ptr->b != 2) return 2;
    return 0;
}
