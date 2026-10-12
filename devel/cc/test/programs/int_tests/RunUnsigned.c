        int lt(unsigned a, unsigned b) { return a < b; }
        unsigned shr(unsigned a, int n) { return a >> n; }
        int sar(int a, int n) { return a >> n; }
        int main(void)
        {
            if (lt(4294967295u, 1)) return 1;
            if (shr(0x80000000u, 31) != 1) return 2;
            if (sar(-8, 1) != -4) return 3;
            if (4294967295u / 2 != 2147483647u) return 4;
            return 0;
        }
    
