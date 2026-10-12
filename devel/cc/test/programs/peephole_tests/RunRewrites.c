        struct S { char a, b; short c; int d; long long e; };
        static int calls;
        static int bump(int x) { calls += x; return calls; }
        static int keep(int *p) { int a = *p; *p = a * 2; return a + *p; }
        int main(void)
        {
            struct S s = { 1, -2, 300, -4, 5 };
            unsigned char *b = (unsigned char *)&s;
            if (b[0] != 1 || b[1] != 254 || b[2] != 44 || b[3] != 1 || b[4] != 252)
                return 1;
            if (s.a + s.b + s.c + s.d + s.e != 300)
                return 2;
            int v = 7;
            if (keep(&v) != 21 || v != 14)
                return 3;
            int g = calls;
            if (g + bump(3) + bump(4) != 10)
                return 4;
            volatile int w = 1;
            int x = w;
            w = 2;
            if (w - x != 1)
                return 5;
            char c[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
            long long sum = 0;
            for (int i = 0; i < 8; i++)
                sum = sum * 10 + c[i];
            return sum == 12345678 ? 0 : 6;
        }
    
