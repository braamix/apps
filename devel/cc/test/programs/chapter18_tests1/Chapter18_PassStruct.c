struct pair { int x; int y; };
int validate_struct_param(struct pair p);
int main(void) {
    struct pair arg = {1, 2};
    if (!validate_struct_param(arg)) return 1;
    return 0;
}
int validate_struct_param(struct pair p) {
    if (p.x != 1 || p.y != 2) return 0;
    return 1;
}

