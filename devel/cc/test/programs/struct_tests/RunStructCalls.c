        struct I { int x; };
        struct D { struct { double d; } in; };
        struct Two { int a, b; };
        struct C3 { char c[3]; };
        struct Big { long long a; char s[20]; double d; };
        union U { int i; char c[4]; };
        struct LD { long double d; };
        struct E { int z[0]; };

        static struct I incI(struct I a) { a.x++; return a; }
        static struct D twice(struct D a) { a.in.d *= 2; return a; }
        static struct Two swap(struct Two t) { int k = t.a; t.a = t.b; t.b = k; return t; }
        static struct C3 rot(struct C3 c) { char k = c.c[0]; c.c[0] = c.c[1]; c.c[1] = c.c[2]; c.c[2] = k; return c; }
        static struct Big grow(struct Big b, int k) { b.a += k; b.s[19] = 'z'; b.d += 0.5; return b; }
        static union U bump(union U u) { u.c[0]++; return u; }
        static struct LD ld(struct LD x, long double y) { struct LD r = x; (void)y; return r; }
        static long double ldv(long double y) { return y; }
        static int empty(int a, struct E e, int b) { (void)e; return a - b; }
        static struct E none(void) { struct E e; return e; }

        int main(void)
        {
            struct I i = { 41 };
            if (incI(i).x != 42 || i.x != 41)
                return 1;
            struct D d = { { 1.5 } };
            if (twice(d).in.d != 3.0)
                return 2;
            struct Two t = { 1, 2 };
            t = swap(t);
            if (t.a != 2 || t.b != 1)
                return 3;
            struct C3 c = { { 'a', 'b', 'c' } };
            c = rot(c);
            if (c.c[0] != 'b' || c.c[2] != 'a')
                return 4;
            struct Big b = { 1, "x", 1.0 };
            struct Big b2 = grow(b, 9);
            if (b2.a != 10 || b2.s[19] != 'z' || b2.d != 1.5 || b.s[19] != 0 || b.a != 1)
                return 5;
            union U u;
            u.i = 0;
            if (bump(bump(u)).c[0] != 2 || u.c[0] != 0)
                return 6;
            struct LD l = { 0 }, m = ld(l, l.d);
            (void)ldv(m.d);
            if (empty(7, none(), 3) != 4)
                return 7;
            struct Two arr[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
            arr[1] = swap(arr[2]);
            if (arr[1].a != 6 || arr[1].b != 5 || arr[2].a != 5)
                return 8;
            return 0;
        }
    
