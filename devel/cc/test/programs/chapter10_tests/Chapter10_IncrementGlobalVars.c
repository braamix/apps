int i = 0;
int j = 0;

int incr_i(void){
    if (i == 1) {
        i++;
        ++i;
    }
    return 0;
}

int decr_j(void) {
    if (j == -1) {
        j--;
    }
    return 0;
}

int main(void) {
    i++ ? 0 : incr_i();
    if (i != 3) {
        return 1;
    }
    --j? decr_j(): 0;
    if (j != -2) {
        return 2;
    }
    return 0;
}
