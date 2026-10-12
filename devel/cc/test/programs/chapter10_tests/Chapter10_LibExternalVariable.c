int x;

extern int x;
int x;

int update_x(int new_val) {
    x = new_val;
    return 0;
}

int read_x(void) {
    return x;
}

int x = 3;

int update_x(int new_val);
int read_x(void);
extern int x;

int main(void) {
    if (x != 3)
        return 1;
    if (read_x() != 3)
        return 1;
    x = 4;
    if (x != 4)
        return 1;
    if (read_x() != 4)
        return 1;
    update_x(5);
    if (x != 5)
        return 1;
    if (read_x() != 5)
        return 1;
    return 0;
}
