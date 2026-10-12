int switch_on_uint(unsigned int ui) {
    switch (ui) {
        case 5u:
            return 0;
        case 4294967286l:
            return 1;
        case 34359738378ul:
            return 2;
        default:
            return 3;
    }
}

int main(void) {
    if (switch_on_uint(5) != 0)
        return 1;
    if (switch_on_uint(4294967286) != 1)
        return 2;
    if (switch_on_uint(34359738378) != 2)
        return 3;
    return 0;
}
