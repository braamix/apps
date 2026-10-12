unsigned int ui = 4294967295;

unsigned int *get_ui_ptr(void){
    return &ui;
}

int shiftcount = 5;

int main(void) {

    if ((*get_ui_ptr() << 2l) != 4294967292) {
        return 1;
    }

    if ((*get_ui_ptr() >> 2) != 1073741823) {
        return 2;
    }

    int *shiftcount_ptr = &shiftcount;
    if ((1000000u >> *shiftcount_ptr) != 31250) {
        return 3;
    }
    if ((1000000u << *shiftcount_ptr) != 32000000) {
        return 4;
    }

    return 0;
}
