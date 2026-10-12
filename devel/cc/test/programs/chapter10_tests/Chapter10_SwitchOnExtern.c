int update_x(void);

int main(void) {
    update_x();
    extern int x;
    switch(x) {
        case 0: return 1;
        case 1: return 2;
        case 4: return 0;
        default: return 4;
    }
}

int x;

int update_x(void) {
    x = 4;
    return 0;
}
