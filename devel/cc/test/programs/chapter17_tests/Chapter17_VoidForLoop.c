/* Test for void expressions in for loop header */

int putch(int c);  // libc (book uses putchar)

int letter;
void initialize_letter(void) {
    letter = 'Z';
}

void decrement_letter(void) {
    letter = letter - 1;
}

int main(void) {
    // void expression in initial condition: print the alphabet backwards
    for (initialize_letter(); letter >= 'A';
         letter = letter - 1) {
        putch(letter);
    }

    // void expression in post condition: print the alphabet forwards
    for (letter = 'A'; letter <= 90; (void)(letter = letter + 1)) {
        putch(letter);
    }

    // void expressions in both conditions: print the alphabet backwards again
    for (initialize_letter(); letter >= 65; decrement_letter()) {
        putch(letter);
    }
    return 0;
}
