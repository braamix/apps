union self_ptr { union self_ptr *ptr; long l; };
int main(void) {
    union self_ptr u = {&u};
    if (&u != u.ptr) return 1;
    return 0;
}
