double d(void) {
    return 1.234e15;
}

int main(void) {
    double retval = d();
    return retval == 1.234e15;
}
