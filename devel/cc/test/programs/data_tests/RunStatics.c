        extern int later;
        signed char c = -5;
        unsigned short us = 65535;
        long long big = 1LL << 40;
        int counter(void) { static int n = 10; return n++; }
        int main(void)
        {
            counter();
            counter();
            if (counter() != 12) return 1;
            if (c != -5 || us != 65535) return 2;
            c = (signed char)200;
            if (c != -56) return 3;
            if (big >> 40 != 1) return 4;
            return later;
        }
        int later = 33;
    
