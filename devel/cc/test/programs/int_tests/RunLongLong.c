        long long mul(long long a, long long b) { return a * b; }
        unsigned long long udiv(unsigned long long a, unsigned long long b) { return a / b; }
        long long shr(long long a, long long n) { return a >> n; }
        int main(void)
        {
            if (mul(1LL << 32, 3) != 12884901888LL) return 1;
            if (udiv(18446744073709551615ULL, 2) != 9223372036854775807ULL) return 2;
            if (shr(-1LL << 40, 39) != -2) return 3;
            if ((int)(mul(0x100000001LL, 1)) != 1) return 4;
            if (-5LL % 3 != -2) return 5;
            return 9;
        }
    
