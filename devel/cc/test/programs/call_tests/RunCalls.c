        _Noreturn void exit(int);
        long long mix(char a, short b, int c, long long d, unsigned char e, int f, int g,
                      int h, int i, long long j)
        {
            return a + b + c + d + e + f + g + h + i + j;
        }
        void deep(int n) { if (n == 0) exit(mix(-1, -2, 3, 1LL << 33, 255, 6, 7, 8, 9, 10) >> 33); deep(n - 1); }
        int main(void) { deep(5); return 1; }
    
