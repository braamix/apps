struct s {
    char x;
    int : 4;
    int y : 6;
    int : 0;
    unsigned long z : 30;
    unsigned w : 9;
};

struct outer {
    int n;
    struct s in;
    unsigned flag : 1;
};

union u {
    int i : 7;
    unsigned char c;
};

struct s g1 = { 'k', -20, 123456789ul, 300 };
struct s g2 = { .w = 511, .y = 31 };
struct s garr[3] = { { 'a', 1, 2, 3 }, [2] = { .z = 99 } };
struct outer go = { 7, { 'b', -1, 5, 6 }, 1 };
union u gu = { -5 };
static struct s gs;

int same(struct s *a, struct s *b) {
    return a->x == b->x && a->y == b->y && a->z == b->z && a->w == b->w;
}

int main(void) {
    struct s l1 = { 'k', -20, 123456789ul, 300 };
    struct s l2 = { .w = 511, .y = 31 };
    struct s larr[3] = { { 'a', 1, 2, 3 }, [2] = { .z = 99 } };
    struct outer lo = { 7, { 'b', -1, 5, 6 }, 1 };
    union u lu = { -5 };
    struct s byhand;
    byhand.x = 'k';
    byhand.y = -20;
    byhand.z = 123456789ul;
    byhand.w = 300;
    if (!same(&g1, &byhand) || !same(&l1, &byhand))
        return 1;
    if (g1.y != -20 || g1.z != 123456789ul || g1.w != 300 || g1.x != 'k')
        return 2;
    if (g2.x != 0 || g2.y != 31 || g2.z != 0 || g2.w != 511)
        return 3;
    if (!same(&g2, &l2))
        return 4;
    for (int i = 0; i < 3; i++)
        if (!same(&garr[i], &larr[i]))
            return 5;
    if (garr[0].w != 3 || garr[1].y != 0 || garr[2].z != 99)
        return 6;
    if (go.n != 7 || go.in.x != 'b' || go.in.y != -1 || go.in.z != 5 || go.in.w != 6 ||
        go.flag != 1)
        return 7;
    if (lo.n != 7 || !same(&lo.in, &go.in) || lo.flag != 1)
        return 8;
    if (gu.i != -5 || lu.i != -5)
        return 9;
    if (gs.x || gs.y || gs.z || gs.w)
        return 10;
    return 0;
}
