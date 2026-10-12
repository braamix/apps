/* Test finding the common type of void * and other pointer types (it's always
 * void *) */

int main(void) {
    // get a pointer to void (zeroed static storage replaces calloc)
    static unsigned int buf3[3];
    void *void_ptr = buf3;

    // we'll use 'array' a a pointer to a complete object
    unsigned int array[3] = {1, 2, 3};

    // like other pointers, void * can be compared to null pointer constant
    if (void_ptr == 0)
        return 1;

    // compare with ==
    if (void_ptr == array)
        return 2;

    // compare with !=
    if (!(void_ptr != array))
        return 3;

    static void *null_ptr = 0;
    int *my_array = null_ptr ? void_ptr : array;

    int array_element = my_array[1];

    if (array_element != 2) {
        return 4;
    }

    return 0;
}
