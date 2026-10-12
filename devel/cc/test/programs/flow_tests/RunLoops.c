        int main(void)
        {
            int s = 0, i = 0;
            while (i < 10) { if (i == 7) break; s += i++; }          // 21
            do { s++; } while (s < 25);                               // 25
            for (int j = 0; j < 10; j++) { if (j % 2) continue; s += j; } // 45
            switch (s) { case 44: s = 0; break; case 45: s += 5; default: s++; } // 51
        again:
            if (s < 60) { s += 3; goto again; }                       // 60
            goto out;
            s = 0;
        out:
            return s;
        }
    
