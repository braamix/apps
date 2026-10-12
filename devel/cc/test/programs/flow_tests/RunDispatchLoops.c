        int main(void)
        {
            int s = 0;
            for (int i = 0; i < 10; i++) { if (i % 3 == 0) continue; s += i; } // 27
            switch (s) { case 27: s += 3; break; default: s = 0; }           // 30
            return s;
        }
    
