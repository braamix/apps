int i = 0;

void putch(int c);
int *print_A(void) {
    putch(65);
    return &i;
}

int *print_B(void) {
    putch(66);
    return &i;
}

int main(void) {

    *print_A() += 5;
    if (i != 5) {
        return 1;
    }

    *print_B() += 5l;
    if (i != 10) {
        return 2;
    }

    return 0;
}
