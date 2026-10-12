int x = 10;

int read_x(void){
    return x;
}

static int internal_x = 1;

int read_internal_x(void);
int read_x(void);

int main(void) {
    if (internal_x != 1)
        return 1;
    internal_x = 2;
    if (read_internal_x() != 2)
        return 1;
    if (read_x() != 10)
        return 1;
    return 0;
}

int read_internal_x(void) {
    return internal_x;
}
