int main(void) {
    int x = 10;
    switch(x = x + 1) {

    }
    switch(x = x + 1)
    ;
    return x;
}
