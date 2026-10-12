extern unsigned int ui;
unsigned int return_uint(void);
int return_uint_as_signed(void);
long return_uint_as_long(void);

int main(void) {
    if (ui != 4294967200u)
        return 0;

    ui = 1000000000u;

    if (return_uint() != 1000000000u)
        return 0;

    if (return_uint_as_signed() != 1000000000)
        return 0;

    if (return_uint_as_long() != 1000000000l)
        return 0;

    return 1;
}

unsigned int ui = 4294967200u;

unsigned int return_uint(void) {
    return ui;
}

int return_uint_as_signed(void) {
    return ui;
}

long return_uint_as_long(void) {
    return ui;
}
