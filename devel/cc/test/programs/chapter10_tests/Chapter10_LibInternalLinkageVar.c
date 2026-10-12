static int x;

int read_x(void) {
    return x;
}

int update_x(int new_val) {
    x = new_val;
    return 0;
}

static int x = 5;
static int x;

int read_x(void);
int update_x(int new_val);

static int client_x;

int main(void) {
    if (client_x != 0)
        return 1;
    if (read_x() != 5)
        return 1;
    update_x(10);
    if (read_x() != 10)
        return 1;
    if (client_x != 0)
        return 1;
    client_x = 20;
    if (client_x != 20)
        return 1;
    if (read_x() != 10)
        return 1;
    return 0;
}

static int client_x;
