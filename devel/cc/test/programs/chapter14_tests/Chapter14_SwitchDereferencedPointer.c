long l = 4294967300l;

long *get_ptr(void) {
    return &l;
}
int main(void) {
    switch (*get_ptr()) {
        case 1:
            return 1;
        case 4:
            return 2;
        case 4294967300l:
            return 0;
        case 1000000000000l:
            return 3;
        default:
            return 4;
    }
}
