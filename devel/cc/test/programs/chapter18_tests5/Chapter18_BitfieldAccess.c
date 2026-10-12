struct flags {
    unsigned ready : 1;
    unsigned mode : 3;
    int level : 5;
};

struct device {
    char name;
    struct flags f[2];
};

union reg {
    struct flags f;
    unsigned char bits;
};

struct flags make(int mode, int level) {
    struct flags r;
    r.ready = 1;
    r.mode = mode;
    r.level = level;
    return r;
}

int sum(struct flags f) {
    return f.ready + f.mode + f.level;
}

int main(void) {
    struct device d;
    struct device *p = &d;
    d.name = 'D';
    for (int i = 0; i < 2; i++) {
        p->f[i].ready = i;
        p->f[i].mode = 5 + i;
        p->f[i].level = -3 - i;
    }
    if (d.f[0].ready != 0 || d.f[0].mode != 5 || d.f[0].level != -3)
        return 1;
    if (d.f[1].ready != 1 || d.f[1].mode != 6 || d.f[1].level != -4)
        return 2;
    if (d.name != 'D')
        return 3;
    if (make(3, -9).level != -9 || make(3, -9).mode != 3)
        return 4;
    if (sum(make(7, 15)) != 23 || sum(d.f[1]) != 3)
        return 5;
    struct flags copy = d.f[1];
    copy.mode++;
    if (copy.mode != 7 || d.f[1].mode != 6)
        return 6;
    union reg r;
    r.bits = 0;
    r.f.ready = 1;
    if (r.bits == 0)
        return 7;
    if (r.f.mode != 0 || r.f.ready != 1)
        return 8;
    return 0;
}
