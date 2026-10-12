        int main(void)
        {
            signed char c = 127;
            unsigned char u = 255;
            short s = 32767;
            unsigned short us = 0;
            c++;
            u++;
            s++;
            us--;
            if (c != -128) return 1;
            if (u != 0) return 2;
            if (s != -32768) return 3;
            if (us != 65535) return 4;
            if ((unsigned)(signed char)200 != 4294967240u) return 5;
            if ((int)(unsigned char)-1 != 255) return 6;
            if ((char)'\x80' >= 0) return 7; // plain char is signed
            return 42;
        }
    
