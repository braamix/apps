long add_variables(long x, long y, int z);

int main(void) {
    long x = 3;
    long y = 4;
    int z = 5;
    return add_variables(x, y, z);
}

long add_variables(long x, long y, int z){
    return x + y + z;
}
