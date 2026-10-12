/* Make sure we accurately calculate the size of derived (pointer and array)
 * types */

int main(void) {
    // start with a simple array type
    // 2 * 4 = 8; sizeof int is 4
    if (sizeof(int[2]) != 8) {
        return 1;
    }

    // try a nested array type
    // 3 * 6 * 17 * 9 == 2754; sizeof char is 1
    if (sizeof(char[3][6][17][9]) != 2754) {
        return 2;
    }

    // now try some pointer types; these are always 8 bytes no matter what they
    // point to
    if (sizeof(int *) != 8) {
        return 4;
    }

    if (sizeof(int(*)[2][4][6]) !=
        8) {  // pointer to a big array is still a pointer
        return 5;
    }

    if (sizeof(char *) != 8) {
        return 6;
    }

    // array of pointers
    // this is an array of three arrays of four pointers; 3 * 4 * 8 = 96
    // each pointer points to element type "array of four doubles"
    // but that doesn't impact the size of this type
    if (sizeof(double(*([3][4]))[2]) != 96) {
        return 7;
    }

    return 0;
}
