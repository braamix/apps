struct inner { double d; int i; };
struct outer { char a; char b; struct inner substruct; };
int access_members_through_pointer(struct outer *ptr, int expected_a,
                                   int expected_b, double expected_d, int expected_i);
void update_members_through_pointer(struct outer *ptr, int a, int b, struct inner *inner_ptr);
int main(void) {
    struct outer s = {1, 2, {3.0, 4}};
    if (!access_members_through_pointer(&s, 1, 2, 3.0, 4)) return 1;
    struct inner inner_struct = {7, 8};
    update_members_through_pointer(&s, 5, 6, &inner_struct);
    if (s.a != 5 || s.b != 6 || s.substruct.d != 7 || s.substruct.i != 8) return 2;
    return 0;
}
int access_members_through_pointer(struct outer *ptr, int expected_a,
                                   int expected_b, double expected_d, int expected_i) {
    if (ptr->a != expected_a) return 0;
    if (ptr->b != expected_b) return 0;
    if (ptr->substruct.d != expected_d) return 0;
    if (ptr->substruct.i != expected_i) return 0;
    return 1;
}
void update_members_through_pointer(struct outer *ptr, int a, int b, struct inner *inner_ptr) {
    ptr->a = a;
    ptr->b = b;
    ptr->substruct = *inner_ptr;
    return;
}

