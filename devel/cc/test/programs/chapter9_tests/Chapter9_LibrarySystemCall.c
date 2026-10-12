void putch(int c);
int incr_and_print(int c);

int main(void) {
    incr_and_print(70);
    return 0;
}

int incr_and_print(int b) {
    putch(b + 2);
    return 0;
}

