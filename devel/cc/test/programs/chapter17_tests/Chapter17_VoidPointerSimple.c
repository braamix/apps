/* A simple test of using statically allocated memory */

int main(void) {
    static int array[10];
    array[2] = 100;
    int result = array[2];
    return result;
}
