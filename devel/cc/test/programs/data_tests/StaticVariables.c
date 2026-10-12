        int g = 5;
        static short s = -2;
        long long ll = -3;
        char c = 'x';
        double d = 1.5;
        float f = 2.0f;
        int z;
        _Alignas(16) int al = 1;
        int main(void) { return g + s + c + z + al; }
    
