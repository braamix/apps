int f(int reg1, int reg2, int reg3, int reg4, int reg5, int reg6,
    int stack1, int stack2, int stack3);

int main(void) {
    return f(1, 2, 3, 4, 5, 6, -1, -2, -3);
}

int f(int reg1, int reg2, int reg3, int reg4, int reg5, int reg6,
    int stack1, int stack2, int stack3) {
    int x = 10;
    if (reg1 == 1 && reg2 == 2 && reg3 == 3 && reg4 == 4 && reg5 == 5
        && reg6 == 6 && stack1 == -1 && stack2 == -2 && stack3 == -3
        && x == 10) {
        stack2 = 100;
        return stack2;
    }
    return 0;
}
