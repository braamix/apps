// Re-coding of advent in C: main program, data initialization, subroutines
// from main, data structure routines, termination routines and privileged
// operations -- upstream's main.c, init.c, subr.c, vocab.c, done.c and
// wizard.c.
//
// Ported to Braam.  exit() does not exist, so a function that ended the game
// returns ADV_OVER and the status travels up to proc_main; and anything that
// reads is a coroutine, because a read is an asynchronous syscall.
#include "hdr.h"

int delhit;
int adv_status; // what proc_main will return
char *wd1, *wd2;
struct game game;
struct travlist *tkk;
const char *magic;

const short setbit[16] = {
    1,    2,     4,     010,   020,    040,    0100,   0200,
    0400, 01000, 02000, 04000, 010000, 020000, 040000, short(0100000),
};

// Data structure routines.

void dstroy(int object)
{
    move(object, 0);
}

void juggle(int object)
{
    int i, j;
    i = game.place[object];
    j = game.fixed[object];
    move(object, i);
    move(object + 100, j);
}

void move(int object, int where)
{
    int from;
    if (object <= 100)
        from = game.place[object];
    else
        from = game.fixed[object - 100];
    if (from > 0 && from <= 300)
        carry(object, from);
    drop(object, where);
}

int put(int object, int where, int pval)
{
    move(object, where);
    return (-1 - pval);
}

void carry(int object, int where)
{
    int temp;
    if (object <= 100) {
        if (game.place[object] == -1)
            return;
        game.place[object] = -1;
        game.holdng++;
    }
    if (game.atloc[where] == object) {
        game.atloc[where] = game.plink[object];
        return;
    }
    for (temp = game.atloc[where]; game.plink[temp] != object; temp = game.plink[temp])
        ;
    game.plink[temp] = game.plink[object];
}

void drop(int object, int where)
{
    if (object > 100)
        game.fixed[object - 100] = where;
    else {
        if (game.place[object] == -1)
            game.holdng--;
        game.place[object] = where;
    }
    if (where <= 0)
        return;
    game.plink[object] = game.atloc[where];
    game.atloc[where]  = object;
}

// Good hash function.
// (C) 2006 Serge Vakulenko
static unsigned int rot13_hash(const char *str)
{
    unsigned int len, hash, c;

    // Max 5 utf8 characters.
    len = 5;
    for (hash = 0; len > 0; str++) {
        c = (unsigned char)*str;
        if (c == 0)
            break;
        if (!(c & 0x80))
            len--;
        hash += (unsigned char)*str;
        hash -= (hash << 13) | (hash >> 19);
    }
    return hash;
}

int vocab(                      // look up or store a word
    const char *word, int type, // -2 for store, -1 for user word, >=0 for canned lookup
    int value)                  // used for storing only
{
    int adr;
    unsigned int hash32, hash;
    struct hashtab *h;

    hash32 = rot13_hash(word); // some kind of hash
    hash   = hash32 % HTSIZE;  // put it into range of table

    for (adr = hash;; adr++) { // look for entry in table
        if (adr == HTSIZE)
            adr = 0;        // wrap around
        h = &game.voc[adr]; // point at the entry
        switch (type) {
        case -2:        // fill in entry
            if (h->val) // already got an entry?
                goto exitloop2;
            h->val  = value;
            h->hash = hash32;
            return 0; // entry unused

        case -1:             // looking up user word
            if (h->val == 0) // not found
                return (-1);
            if ((unsigned int)h->hash != hash32)
                goto exitloop2;
            return h->val; // the word matched o.k.

        default: // looking up known word
            if (h->val == 0) {
                adv_printf("Unable to find %s in vocab\n", word);
                bug(33);
            }
            if ((unsigned int)h->hash != hash32)
                goto exitloop2;
            // the word matched o.k.
            if (h->val / 1000 != type)
                continue;
            return h->val % 1000;
        }
    exitloop2: // hashed entry does not match
        if (adr + 1 == (int)hash || (adr == HTSIZE && hash == 0)) {
            adv_printf("Hash table overflow\n");
            bug(34);
        }
    }
}

void copystr( // copy one string to another
    const char *w1, char *w2)
{
    const char *s;
    char *t;
    for (s = w1, t = w2; *s;)
        *t++ = *s++;
    *t = 0;
}

int weq(                            // compare words
    const char *w1, const char *w2) // w1 is user, w2 is system
{
    const char *s, *t;
    int i;
    s = w1;
    t = w2;
    for (i = 0; i < 5; i++) // compare at most 5 chars
    {
        if (*t == 0 && *s == 0)
            return (TRUE);
        if (*s++ != *t++)
            return (FALSE);
    }
    return (TRUE);
}

int length( // includes 0 at end
    const char *str)
{
    const char *s;
    int n;
    for (n = 0, s = str; *s++;)
        n++;
    return (n + 1);
}

// Data initialization.

void linkdata(void) // secondary data manipulation
{
    int i, j;
    // array linkages
    for (i = 1; i <= LOCSIZ; i++)
        if (game.ltext[i].seekadr != 0 && game.travel[i] != 0)
            if ((game.travel[i]->tverb) == 1)
                game.cond[i] = 2;
    for (j = 100; j > 0; j--)
        if (game.fixd[j] > 0) {
            drop(j + 100, game.fixd[j]);
            drop(j, game.plac[j]);
        }
    for (j = 100; j > 0; j--) {
        game.fixed[j] = game.fixd[j];
        if (game.plac[j] != 0 && game.fixd[j] <= 0)
            drop(j, game.plac[j]);
    }

    game.maxtrs = 79;
    game.tally  = 0;
    game.tally2 = 0;

    for (i = 50; i <= game.maxtrs; i++) {
        if (game.ptext[i].seekadr != 0)
            game.prop[i] = -1;
        game.tally -= game.prop[i];
    }

    // define mnemonics
    game.keys   = vocab("keys", 1, 0);
    game.lamp   = vocab("lamp", 1, 0);
    game.grate  = vocab("grate", 1, 0);
    game.cage   = vocab("cage", 1, 0);
    game.rod    = vocab("rod", 1, 0);
    game.rod2   = game.rod + 1;
    game.steps  = vocab("steps", 1, 0);
    game.bird   = vocab("bird", 1, 0);
    game.door   = vocab("door", 1, 0);
    game.pillow = vocab("pillow", 1, 0);
    game.snake  = vocab("snake", 1, 0);
    game.fissur = vocab("fissu", 1, 0);
    game.tablet = vocab("table", 1, 0);
    game.clam   = vocab("clam", 1, 0);
    game.oyster = vocab("oyster", 1, 0);
    game.magzin = vocab("magaz", 1, 0);
    game.dwarf  = vocab("dwarf", 1, 0);
    game.knife  = vocab("knife", 1, 0);
    game.food   = vocab("food", 1, 0);
    game.bottle = vocab("bottl", 1, 0);
    game.water  = vocab("water", 1, 0);
    game.oil    = vocab("oil", 1, 0);
    game.plant  = vocab("plant", 1, 0);
    game.plant2 = game.plant + 1;
    game.axe    = vocab("axe", 1, 0);
    game.mirror = vocab("mirro", 1, 0);
    game.dragon = vocab("drago", 1, 0);
    game.chasm  = vocab("chasm", 1, 0);
    game.troll  = vocab("troll", 1, 0);
    game.troll2 = game.troll + 1;
    game.bear   = vocab("bear", 1, 0);
    game.messag = vocab("messa", 1, 0);
    game.vend   = vocab("vendi", 1, 0);
    game.batter = vocab("batte", 1, 0);

    game.nugget = vocab("gold", 1, 0);
    game.coins  = vocab("coins", 1, 0);
    game.chest  = vocab("chest", 1, 0);
    game.eggs   = vocab("eggs", 1, 0);
    game.tridnt = vocab("tride", 1, 0);
    game.vase   = vocab("vase", 1, 0);
    game.emrald = vocab("emera", 1, 0);
    game.pyram  = vocab("pyram", 1, 0);
    game.pearl  = vocab("pearl", 1, 0);
    game.rug    = vocab("rug", 1, 0);
    game.chain  = vocab("chain", 1, 0);

    game.back   = vocab("back", 0, 0);
    game.look   = vocab("look", 0, 0);
    game.cave   = vocab("cave", 0, 0);
    game.null   = vocab("null", 0, 0);
    game.entrnc = vocab("entra", 0, 0);
    game.dprssn = vocab("depre", 0, 0);

    game.say    = vocab("say", 2, 0);
    game.lock   = vocab("lock", 2, 0);
    game.throw_ = vocab("throw", 2, 0);
    game.find   = vocab("find", 2, 0);
    game.invent = vocab("inven", 2, 0);
    // initialize dwarves
    game.chloc  = 114;
    game.chloc2 = 140;
    for (i = 1; i <= 6; i++)
        game.dseen[i] = FALSE;
    game.dflag   = 0;
    game.dloc[1] = 19;
    game.dloc[2] = 27;
    game.dloc[3] = 33;
    game.dloc[4] = 44;
    game.dloc[5] = 64;
    game.dloc[6] = game.chloc;
    game.daltlc  = 18;

    // random flags & ctrs
    game.turns  = 0;
    game.lmwarn = FALSE;
    game.iwest  = 0;
    game.knfloc = 0;
    game.detail = 0;
    game.abbnum = 5;
    for (i = 0; i <= 4; i++)
        if (game.rtext[2 * i + 81].seekadr != 0)
            game.maxdie = i + 1;
    game.numdie = game.holdng = game.dkill = game.foobar = game.bonus = 0;
    game.clock1                                                       = 30;
    game.clock2                                                       = 50;
    game.saved                                                        = 0;
    game.closng = game.panic = game.closed = game.scorng = FALSE;
}

Task<i32> startup(void)
{
    if (Task<void> t = adv_seed()) // random odd seed
        co_await t;
    game.hinted[3] = co_await yes(65, 1, 0);
    game.newloc    = 1;
    game.limit     = 330;
    if (game.hinted[3])
        game.limit = 1000; // better batteries if instrucs
    co_return 0;
}

// Privileged operations.

void poof(void)
{
    magic       = "dwarf";
    game.latncy = 15; // originally 45 minutes
}

static Task<i32> wizard(void) // not as complex as advent/10 (for now)
{
    char *word, *x;
    if (!co_await yesm(16, 0, 7))
        co_return FALSE;
    mspeak(17);
    if (Task<void> t = getin(&word, &x))
        co_await t;
    if (!weq(word, magic)) {
        mspeak(20);
        co_return FALSE;
    }
    mspeak(19);
    co_return TRUE;
}

Task<i32> start(int n)
{
    int d, t, delay;

    (void)n;
    if (Task<void> c = adv_clock())
        co_await c;
    datime(&d, &t);
    delay = (d - game.saved) * 1440 + (t - game.savet); // good for about a month
    if (delay >= game.latncy || delay < 0) {
        game.saved = -1;
        co_return 0;
    }
    adv_printf("This adventure was suspended a mere %d minutes ago.", delay);
    if (delay <= game.latncy / 3) {
        mspeak(2);
        adv_status = 0;
        co_return ADV_OVER;
    }
    mspeak(8);
    if (!co_await wizard()) {
        mspeak(9);
        adv_status = 0;
        co_return ADV_OVER;
    }
    game.saved = -1;
    co_return 0;
}

Task<i32> ciao(void)
{
    String fname;

    for (;;) {
        adv_printf("What would you like to call the saved version?\n");
        if (Task<void> t = adv_flush())
            co_await t;
        AdvEnd how = AdvEnd::Eof;
        if (Task<AdvEnd> t = adv_readline(fname))
            how = co_await t;
        if (how != AdvEnd::Enter || fname.empty()) {
            adv_status = 0;
            co_return ADV_OVER;
        }
        Result<FileInfo> st = Err(Error::NotFound);
        if (Task<Result<FileInfo>> t = stat_of(fname.str()))
            st = co_await t;
        if (st.is_err())
            break;
        adv_printf("I can't use that one.\n");
        co_return 0;
    }
    if (Task<Result<void>> t = save(fname.str()))
        co_await t;
    adv_printf("                    ^\n");
    adv_printf("That should do it.  Gis revido.\n");
    adv_status = 0;
    co_return ADV_OVER;
}

// Termination routines.

int score(void) // sort of like 20000
{
    int scor, i;
    game.mxscor = scor = 0;
    for (i = 50; i <= game.maxtrs; i++) {
        if (game.ptext[i].txtlen == 0)
            continue;
        game.k = 12;
        if (i == game.chest)
            game.k = 14;
        if (i > game.chest)
            game.k = 16;
        if (game.prop[i] >= 0)
            scor += 2;
        if (game.place[i] == 3 && game.prop[i] == 0)
            scor += game.k - 2;
        game.mxscor += game.k;
    }
    scor += (game.maxdie - game.numdie) * 10;
    game.mxscor += game.maxdie * 10;
    if (!(game.scorng || game.gaveup))
        scor += 4;
    game.mxscor += 4;
    if (game.dflag != 0)
        scor += 25;
    game.mxscor += 25;
    if (game.closng)
        scor += 25;
    game.mxscor += 25;
    if (game.closed) {
        if (game.bonus == 0)
            scor += 10;
        if (game.bonus == 135)
            scor += 25;
        if (game.bonus == 134)
            scor += 30;
        if (game.bonus == 133)
            scor += 45;
    }
    game.mxscor += 45;
    if (game.place[game.magzin] == 108)
        scor++;
    game.mxscor++;
    scor += 2;
    game.mxscor += 2;
    for (i = 1; i <= game.hntmax; i++)
        if (game.hinted[i])
            scor -= game.hints[i][2];
    return (scor);
}

// game is over
// entry=1 means goto 13000, entry=2 means goto 20000, 3=19000
Task<i32> done(int entry)
{
    int i, sc;
    if (entry == 1)
        mspeak(1);
    if (entry == 3)
        rspeak(136);
    adv_printf("\n\n\nYou scored %d out of a ", (sc = score()));
    adv_printf("possible %d using %d turns.\n", game.mxscor, game.turns);
    adv_status = 0;
    for (i = 1; i <= game.clsses; i++)
        if (game.cval[i] >= sc) {
            speak(&game.ctext[i]);
            if (i == game.clsses - 1) {
                adv_printf("To achieve the next higher rating");
                adv_printf(" would be a neat trick!\n\n");
                adv_printf("Congratulations!!\n");
                co_return ADV_OVER;
            }
            game.k = game.cval[i] + 1 - sc;
            adv_printf("To achieve the next higher rating, you need");
            adv_printf(" %d more point", game.k);
            if (game.k == 1)
                adv_printf(".\n");
            else
                adv_printf("s.\n");
            co_return ADV_OVER;
        }
    adv_printf("You just went off my scale!!!\n");
    co_return ADV_OVER;
}

Task<i32> die( // label 90
    int entry)
{
    int i, yea;
    if (entry != 99) {
        rspeak(23);
        game.oldlc2 = game.loc;
    }
    if (game.closng) // 99
    {
        rspeak(131);
        game.numdie++;
        co_return co_await done(2);
    }
    yea = co_await yes(81 + game.numdie * 2, 82 + game.numdie * 2, 54);
    game.numdie++;
    if (game.numdie == game.maxdie || !yea)
        co_return co_await done(2);
    game.place[game.water] = 0;
    game.place[game.oil]   = 0;
    if (toting(game.lamp))
        game.prop[game.lamp] = 0;
    for (i = 100; i >= 1; i--) {
        if (!toting(i))
            continue;
        game.k = game.oldlc2;
        if (i == game.lamp)
            game.k = 1;
        drop(i, game.k);
    }
    game.loc    = 3;
    game.oldloc = game.loc;
    co_return 2000;
}

// Subroutines from main.

// Statement functions

int toting(int objj)
{
    if (game.place[objj] == -1)
        return (TRUE);
    else
        return (FALSE);
}

int here(int objj)
{
    if (game.place[objj] == game.loc || toting(objj))
        return (TRUE);
    else
        return (FALSE);
}

int at(int objj)
{
    if (game.place[objj] == game.loc || game.fixed[objj] == game.loc)
        return (TRUE);
    else
        return (FALSE);
}

static int liq2(int pbotl)
{
    return ((1 - pbotl) * game.water + (pbotl / 2) * (game.water + game.oil));
}

int liq(int foo)
{
    int i;
    (void)foo;
    i = game.prop[game.bottle];
    if (i > -1 - i)
        return (liq2(i));
    else
        return (liq2(-1 - i));
}

int liqloc(int locc) // may want to clean this one up a bit
{
    int i, j, l;
    i = game.cond[locc] / 2;
    j = ((i * 2) % 8) - 5;
    l = game.cond[locc] / 4;
    l = l % 2;
    return (liq2(j * l + 1));
}

static int bitset(int l, int n)
{
    if (game.cond[l] & setbit[n])
        return (TRUE);
    return (FALSE);
}

int forced(int locc)
{
    if (game.cond[locc] == 2)
        return (TRUE);
    return (FALSE);
}

int dark(int foo)
{
    (void)foo;
    if ((game.cond[game.loc] % 2) == 0 && (game.prop[game.lamp] == 0 || !here(game.lamp)))
        return (TRUE);
    return (FALSE);
}

int pct(int n)
{
    if (ran(100) < n)
        return (TRUE);
    return (FALSE);
}

int fdwarf(void) // 71
{
    int i, j;
    struct travlist *kk;

    if (game.newloc != game.loc && !forced(game.loc) && !bitset(game.loc, 3)) {
        for (i = 1; i <= 5; i++) {
            if (game.odloc[i] != game.newloc || !game.dseen[i])
                continue;
            game.newloc = game.loc;
            rspeak(2);
            break;
        }
    }
    game.loc = game.newloc; // 74
    if (game.loc == 0 || forced(game.loc) || bitset(game.newloc, 3))
        return (2000);
    if (game.dflag == 0) {
        if (game.loc >= 15)
            game.dflag = 1;
        return (2000);
    }
    if (game.dflag == 1) // 6000
    {
        if (game.loc < 15 || pct(95))
            return (2000);
        game.dflag = 2;
        for (i = 1; i <= 2; i++) {
            j = 1 + ran(5);
            if (pct(50) && game.saved == -1)
                game.dloc[j] = 0; // 6001
        }
        for (i = 1; i <= 5; i++) {
            if (game.dloc[i] == game.loc)
                game.dloc[i] = game.daltlc;
            game.odloc[i] = game.dloc[i]; // 6002
        }
        rspeak(3);
        drop(game.axe, game.loc);
        return (2000);
    }
    game.dtotal = game.attack = game.stick = 0; // 6010
    for (i = 1; i <= 6; i++)                    // loop to 6030
    {
        if (game.dloc[i] == 0)
            continue;
        j = 1;
        for (kk = game.travel[game.dloc[i]]; kk != 0; kk = kk->next) {
            game.newloc = kk->tloc;
            if (game.newloc > 300 || game.newloc < 15 || game.newloc == game.odloc[i] ||
                (j > 1 && game.newloc == game.tk[j - 1]) || j >= 20 ||
                game.newloc == game.dloc[i] || forced(game.newloc) ||
                (i == 6 && bitset(game.newloc, 3)) || kk->conditions == 100)
                continue;
            game.tk[j++] = game.newloc;
        }
        game.tk[j] = game.odloc[i]; // 6016
        if (j >= 2)
            j--;
        j             = 1 + ran(j);
        game.odloc[i] = game.dloc[i];
        game.dloc[i]  = game.tk[j];
        game.dseen[i] = (game.dseen[i] && game.loc >= 15) ||
                        (game.dloc[i] == game.loc || game.odloc[i] == game.loc);
        if (!game.dseen[i])
            continue; // i.e. goto 6030
        game.dloc[i] = game.loc;
        if (i == 6) // pirate's spotted him
        {
            if (game.loc == game.chloc || game.prop[game.chest] >= 0)
                continue;
            game.k = 0;
            for (j = 50; j <= game.maxtrs; j++) // loop to 6020
            {
                if (j == game.pyram &&
                    (game.loc == game.plac[game.pyram] || game.loc == game.plac[game.emrald]))
                    goto l6020;
                if (toting(j))
                    goto l6022;
            l6020:
                if (here(j))
                    game.k = 1;
            } // 6020
            if (game.tally == game.tally2 + 1 && game.k == 0 && game.place[game.chest] == 0 &&
                here(game.lamp) && game.prop[game.lamp] == 1)
                goto l6025;
            if (game.odloc[6] != game.dloc[6] && pct(20))
                rspeak(127);
            continue; // to 6030
        l6022:
            rspeak(128);
            if (game.place[game.messag] == 0)
                move(game.chest, game.chloc);
            move(game.messag, game.chloc2);
            for (j = 50; j <= game.maxtrs; j++) // loop to 6023
            {
                if (j == game.pyram &&
                    (game.loc == game.plac[game.pyram] || game.loc == game.plac[game.emrald]))
                    continue;
                if (at(j) && game.fixed[j] == 0)
                    carry(j, game.loc);
                if (toting(j))
                    drop(j, game.chloc);
            }
        l6024:
            game.dloc[6] = game.odloc[6] = game.chloc;
            game.dseen[6]                = FALSE;
            continue;
        l6025:
            rspeak(186);
            move(game.chest, game.chloc);
            move(game.messag, game.chloc2);
            goto l6024;
        }
        game.dtotal++; // 6027
        if (game.odloc[i] != game.dloc[i])
            continue;
        game.attack++;
        if (game.knfloc >= 0)
            game.knfloc = game.loc;
        if (ran(1000) < 95 * (game.dflag - 2))
            game.stick++;
    } // 6030
    if (game.dtotal == 0)
        return (2000);
    if (game.dtotal != 1) {
        adv_printf("There are %d threatening little dwarves ", game.dtotal);
        adv_printf("in the room with you.\n");
    } else
        rspeak(4);
    if (game.attack == 0)
        return (2000);
    if (game.dflag == 2)
        game.dflag = 3;
    if (game.saved != -1)
        game.dflag = 20;
    if (game.attack != 1) {
        adv_printf("%d of them throw knives at you!\n", game.attack);
        game.k = 6;
    l82:
        if (game.stick <= 1) // 82
        {
            rspeak(game.k + game.stick);
            if (game.stick == 0)
                return (2000);
        } else
            adv_printf("%d of them get you!\n", game.stick); // 83
        game.oldlc2 = game.loc;
        return (99);
    }
    rspeak(5);
    game.k = 52;
    goto l82;
}

static int mback(void) // 20
{
    struct travlist *tk2, *j;
    int ll;
    if (forced(game.k = game.oldloc))
        game.k = game.oldlc2; // k=location
    game.oldlc2 = game.oldloc;
    game.oldloc = game.loc;
    tk2         = 0;
    if (game.k == game.loc) {
        rspeak(91);
        return (2);
    }
    for (; tkk != 0; tkk = tkk->next) // 21
    {
        ll = tkk->tloc;
        if (ll == game.k) {
            game.k = tkk->tverb; // k back to verb
            tkk    = game.travel[game.loc];
            return (9);
        }
        if (ll <= 300) {
            j = game.travel[game.loc];
            if (forced(ll) && game.k == j->tloc)
                tk2 = tkk;
        }
    }
    tkk = tk2; // 23
    if (tkk != 0) {
        game.k = tkk->tverb;
        tkk    = game.travel[game.loc];
        return (9);
    }
    rspeak(140);
    return (2);
}

static int badmove(void) // 20
{
    game.spk = 12;
    if (game.k >= 43 && game.k <= 50)
        game.spk = 9;
    if (game.k == 29 || game.k == 30)
        game.spk = 9;
    if (game.k == 7 || game.k == 36 || game.k == 37)
        game.spk = 10;
    if (game.k == 11 || game.k == 19)
        game.spk = 11;
    if (game.verb == game.find || game.verb == game.invent)
        game.spk = 59;
    if (game.k == 62 || game.k == 65)
        game.spk = 42;
    if (game.k == 17)
        game.spk = 80;
    rspeak(game.spk);
    return (2);
}

static int trbridge(void) // 30300
{
    if (game.prop[game.troll] == 1) {
        pspeak(game.troll, 1);
        game.prop[game.troll] = 0;
        move(game.troll2, 0);
        move(game.troll2 + 100, 0);
        move(game.troll, game.plac[game.troll]);
        move(game.troll + 100, game.fixd[game.troll]);
        juggle(game.chasm);
        game.newloc = game.loc;
        return (2);
    }
    game.newloc = game.plac[game.troll] + game.fixd[game.troll] - game.loc; // 30310
    if (game.prop[game.troll] == 0)
        game.prop[game.troll] = 1;
    if (!toting(game.bear))
        return (2);
    rspeak(162);
    game.prop[game.chasm] = 1;
    game.prop[game.troll] = 2;
    drop(game.bear, game.newloc);
    game.fixed[game.bear] = -1;
    game.prop[game.bear]  = 3;
    if (game.prop[game.spices] < 0)
        game.tally2++;
    game.oldlc2 = game.newloc;
    return (99);
}

static int specials(void) // 30000
{
    switch (game.newloc -= 300) {
    case 1: // 30100
        game.newloc = 99 + 100 - game.loc;
        if (game.holdng == 0 || (game.holdng == 1 && toting(game.emrald)))
            return (2);
        game.newloc = game.loc;
        rspeak(117);
        return (2);
    case 2: // 30200
        drop(game.emrald, game.loc);
        return (12);
    case 3: // to 30300
        return (trbridge());
    default:
        bug(29);
    }
}

int march(void) // label 8
{
    int ll1, ll2;

    tkk = game.travel[game.newloc = game.loc];
    if (tkk == 0)
        bug(26);
    if (game.k == game.null)
        return (2);
    if (game.k == game.cave) // 40
    {
        if (game.loc < 8)
            rspeak(57);
        if (game.loc >= 8)
            rspeak(58);
        return (2);
    }
    if (game.k == game.look) // 30
    {
        if (game.detail++ < 3)
            rspeak(15);
        game.wzdark        = FALSE;
        game.abb[game.loc] = 0;
        return (2);
    }
    if (game.k == game.back) // 20
    {
        switch (mback()) {
        case 2:
            return (2);
        case 9:
            goto l9;
        default:
            bug(100);
        }
    }
    game.oldlc2 = game.oldloc;
    game.oldloc = game.loc;
l9:
    for (; tkk != 0; tkk = tkk->next)
        if (tkk->tverb == 1 || tkk->tverb == game.k)
            break;
    if (tkk == 0) {
        badmove();
        return (2);
    }
l11:
    ll1         = tkk->conditions; // 11
    ll2         = tkk->tloc;
    game.newloc = ll1;               // newloc=conditions
    game.k      = game.newloc % 100; // k used for prob
    if (game.newloc <= 300) {
        if (game.newloc <= 100) // 13
        {
            if (game.newloc != 0 && !pct(game.newloc))
                goto l12; // 14
        l16:
            game.newloc = ll2; // newloc=location
            if (game.newloc <= 300)
                return (2);
            if (game.newloc <= 500)
                switch (specials()) // to 30000
                {
                case 2:
                    return (2);
                case 12:
                    goto l12;
                case 99:
                    return (99);
                default:
                    bug(101);
                }
            rspeak(game.newloc - 500);
            game.newloc = game.loc;
            return (2);
        }
        if (toting(game.k) || (game.newloc > 200 && at(game.k)))
            goto l16;
        goto l12;
    }
    if (game.prop[game.k] != (game.newloc / 100) - 3)
        goto l16; // newloc still conditions
l12:              // alternative to probability move
    for (; tkk != 0; tkk = tkk->next)
        if (tkk->tloc != ll2 || tkk->conditions != ll1)
            break;
    if (tkk == 0)
        bug(25);
    goto l11;
}

Task<void> checkhints(void) // 2600 &c
{
    int hint;
    for (hint = 4; hint <= game.hntmax; hint++) {
        if (game.hinted[hint])
            continue;
        if (!bitset(game.loc, hint))
            game.hintlc[hint] = -1;
        game.hintlc[hint]++;
        if (game.hintlc[hint] < game.hints[hint][1])
            continue;
        switch (hint) {
        case 4: // 40400
            if (game.prop[game.grate] == 0 && !here(game.keys))
                goto l40010;
            goto l40020;
        case 5: // 40500
            if (here(game.bird) && toting(game.rod) && game.obj == game.bird)
                goto l40010;
            continue; // i.e. goto l40030
        case 6:       // 40600
            if (here(game.snake) && !here(game.bird))
                goto l40010;
            goto l40020;
        case 7: // 40700
            if (game.atloc[game.loc] == 0 && game.atloc[game.oldloc] == 0 &&
                game.atloc[game.oldlc2] == 0 && game.holdng > 1)
                goto l40010;
            goto l40020;
        case 8: // 40800
            if (game.prop[game.emrald] != -1 && game.prop[game.pyram] == -1)
                goto l40010;
            goto l40020;
        case 9:
            goto l40010; // 40900
        default:
            bug(27);
        }
    l40010:
        game.hintlc[hint] = 0;
        if (!co_await yes(game.hints[hint][3], 0, 54))
            continue;
        adv_printf("I am prepared to give you a hint, but it will ");
        adv_printf("cost you %d points.\n", game.hints[hint][2]);
        game.hinted[hint] = co_await yes(175, game.hints[hint][4], 54);
    l40020:
        game.hintlc[hint] = 0;
    }
    co_return;
}

int trsay(void) // 9030
{
    int i;
    if (*wd2 != 0)
        copystr(wd2, wd1);
    i = vocab(wd1, -1, 0);
    if (i == 62 || i == 65 || i == 71 || i == 2025) {
        *wd2     = 0;
        game.obj = 0;
        return (2630);
    }
    adv_printf("\nOkay, \"%s\".\n", wd2);
    return (2012);
}

int trtake(void) // 9010
{
    if (toting(game.obj))
        return (2011); // 9010
    game.spk = 25;
    if (game.obj == game.plant && game.prop[game.plant] <= 0)
        game.spk = 115;
    if (game.obj == game.bear && game.prop[game.bear] == 1)
        game.spk = 169;
    if (game.obj == game.chain && game.prop[game.bear] != 0)
        game.spk = 170;
    if (game.fixed[game.obj] != 0)
        return (2011);
    if (game.obj == game.water || game.obj == game.oil) {
        if (here(game.bottle) && liq(0) == game.obj) {
            game.obj = game.bottle;
            goto l9017;
        }
        game.obj = game.bottle;
        if (toting(game.bottle) && game.prop[game.bottle] == 1)
            return (9220);
        if (game.prop[game.bottle] != 1)
            game.spk = 105;
        if (!toting(game.bottle))
            game.spk = 104;
        return (2011);
    }
l9017:
    if (game.holdng >= 7) {
        rspeak(92);
        return (2012);
    }
    if (game.obj == game.bird) {
        if (game.prop[game.bird] != 0)
            goto l9014;
        if (toting(game.rod)) {
            rspeak(26);
            return (2012);
        }
        if (!toting(game.cage)) // 9013
        {
            rspeak(27);
            return (2012);
        }
        game.prop[game.bird] = 1; // 9015
    }
l9014:
    if ((game.obj == game.bird || game.obj == game.cage) && game.prop[game.bird] != 0)
        carry(game.bird + game.cage - game.obj, game.loc);
    carry(game.obj, game.loc);
    game.k = liq(0);
    if (game.obj == game.bottle && game.k != 0)
        game.place[game.k] = -1;
    return (2009);
}

static int dropper(void) // 9021
{
    game.k = liq(0);
    if (game.k == game.obj)
        game.obj = game.bottle;
    if (game.obj == game.bottle && game.k != 0)
        game.place[game.k] = 0;
    if (game.obj == game.cage && game.prop[game.bird] != 0)
        drop(game.bird, game.loc);
    if (game.obj == game.bird)
        game.prop[game.bird] = 0;
    drop(game.obj, game.loc);
    return (2012);
}

int trdrop(void) // 9020
{
    if (toting(game.rod2) && game.obj == game.rod && !toting(game.rod))
        game.obj = game.rod2;
    if (!toting(game.obj))
        return (2011);
    if (game.obj == game.bird && here(game.snake)) {
        rspeak(30);
        if (game.closed)
            return (19000);
        dstroy(game.snake);
        game.prop[game.snake] = 1;
        return (dropper());
    }
    if (game.obj == game.coins && here(game.vend)) // 9024
    {
        dstroy(game.coins);
        drop(game.batter, game.loc);
        pspeak(game.batter, 0);
        return (2012);
    }
    if (game.obj == game.bird && at(game.dragon) && game.prop[game.dragon] == 0) // 9025
    {
        rspeak(154);
        dstroy(game.bird);
        game.prop[game.bird] = 0;
        if (game.place[game.snake] == game.plac[game.snake])
            game.tally2--;
        return (2012);
    }
    if (game.obj == game.bear && at(game.troll)) // 9026
    {
        rspeak(163);
        move(game.troll, 0);
        move(game.troll + 100, 0);
        move(game.troll2, game.plac[game.troll]);
        move(game.troll2 + 100, game.fixd[game.troll]);
        juggle(game.chasm);
        game.prop[game.troll] = 2;
        return (dropper());
    }
    if (game.obj != game.vase || game.loc == game.plac[game.pillow]) // 9027
    {
        rspeak(54);
        return (dropper());
    }
    game.prop[game.vase] = 2; // 9028
    if (at(game.pillow))
        game.prop[game.vase] = 0;
    pspeak(game.vase, game.prop[game.vase] + 1);
    if (game.prop[game.vase] != 0)
        game.fixed[game.vase] = -1;
    return (dropper());
}

int tropen(void) // 9040
{
    if (game.obj == game.clam || game.obj == game.oyster) {
        game.k = 0; // 9046
        if (game.obj == game.oyster)
            game.k = 1;
        game.spk = 124 + game.k;
        if (toting(game.obj))
            game.spk = 120 + game.k;
        if (!toting(game.tridnt))
            game.spk = 122 + game.k;
        if (game.verb == game.lock)
            game.spk = 61;
        if (game.spk != 124)
            return (2011);
        dstroy(game.clam);
        drop(game.oyster, game.loc);
        drop(game.pearl, 105);
        return (2011);
    }
    if (game.obj == game.door)
        game.spk = 111;
    if (game.obj == game.door && game.prop[game.door] == 1)
        game.spk = 54;
    if (game.obj == game.cage)
        game.spk = 32;
    if (game.obj == game.keys)
        game.spk = 55;
    if (game.obj == game.grate || game.obj == game.chain)
        game.spk = 31;
    if (game.spk != 31 || !here(game.keys))
        return (2011);
    if (game.obj == game.chain) {
        if (game.verb == game.lock) {
            game.spk = 172; // 9049: lock
            if (game.prop[game.chain] != 0)
                game.spk = 34;
            if (game.loc != game.plac[game.chain])
                game.spk = 173;
            if (game.spk != 172)
                return (2011);
            game.prop[game.chain] = 2;
            if (toting(game.chain))
                drop(game.chain, game.loc);
            game.fixed[game.chain] = -1;
            return (2011);
        }
        game.spk = 171;
        if (game.prop[game.bear] == 0)
            game.spk = 41;
        if (game.prop[game.chain] == 0)
            game.spk = 37;
        if (game.spk != 171)
            return (2011);
        game.prop[game.chain]  = 0;
        game.fixed[game.chain] = 0;
        if (game.prop[game.bear] != 3)
            game.prop[game.bear] = 2;
        game.fixed[game.bear] = 2 - game.prop[game.bear];
        return (2011);
    }
    if (game.closng) {
        game.k = 130;
        if (!game.panic)
            game.clock2 = 15;
        game.panic = TRUE;
        return (2010);
    }
    game.k                = 34 + game.prop[game.grate]; // 9043
    game.prop[game.grate] = 1;
    if (game.verb == game.lock)
        game.prop[game.grate] = 0;
    game.k = game.k + 2 * game.prop[game.grate];
    return (2010);
}

Task<i32> trkill(void) // 9120
{
    int i;
    for (i = 1; i <= 5; i++)
        if (game.dloc[i] == game.loc && game.dflag >= 2)
            break;
    if (i == 6)
        i = 0;
    if (game.obj == 0) // 9122
    {
        if (i != 0)
            game.obj = game.dwarf;
        if (here(game.snake))
            game.obj = game.obj * 100 + game.snake;
        if (at(game.dragon) && game.prop[game.dragon] == 0)
            game.obj = game.obj * 100 + game.dragon;
        if (at(game.troll))
            game.obj = game.obj * 100 + game.troll;
        if (here(game.bear) && game.prop[game.bear] == 0)
            game.obj = game.obj * 100 + game.bear;
        if (game.obj > 100)
            co_return 8000;
        if (game.obj == 0) {
            if (here(game.bird) && game.verb != game.throw_)
                game.obj = game.bird;
            if (here(game.clam) || here(game.oyster))
                game.obj = 100 * game.obj + game.clam;
            if (game.obj > 100)
                co_return 8000;
        }
    }
    if (game.obj == game.bird) // 9124
    {
        game.spk = 137;
        if (game.closed)
            co_return 2011;
        dstroy(game.bird);
        game.prop[game.bird] = 0;
        if (game.place[game.snake] == game.plac[game.snake])
            game.tally2++;
        game.spk = 45;
    }
    if (game.obj == 0)
        game.spk = 44; // 9125
    if (game.obj == game.clam || game.obj == game.oyster)
        game.spk = 150;
    if (game.obj == game.snake)
        game.spk = 46;
    if (game.obj == game.dwarf)
        game.spk = 49;
    if (game.obj == game.dwarf && game.closed)
        co_return 19000;
    if (game.obj == game.dragon)
        game.spk = 147;
    if (game.obj == game.troll)
        game.spk = 157;
    if (game.obj == game.bear)
        game.spk = 165 + (game.prop[game.bear] + 1) / 2;
    if (game.obj != game.dragon || game.prop[game.dragon] != 0)
        co_return 2011;
    rspeak(49);
    game.verb = 0;
    game.obj  = 0;
    if (Task<void> t = getin(&wd1, &wd2))
        co_await t;
    if (!weq(wd1, "y") && !weq(wd1, "yes"))
        co_return 2608;
    pspeak(game.dragon, 1);
    game.prop[game.dragon] = 2;
    game.prop[game.rug]    = 0;
    game.k                 = (game.plac[game.dragon] + game.fixd[game.dragon]) / 2;
    move(game.dragon + 100, -1);
    move(game.rug + 100, 0);
    move(game.dragon, game.k);
    move(game.rug, game.k);
    for (game.obj = 1; game.obj <= 100; game.obj++)
        if (game.place[game.obj] == game.plac[game.dragon] ||
            game.place[game.obj] == game.fixd[game.dragon])
            move(game.obj, game.k);
    game.loc = game.k;
    game.k   = game.null;
    co_return 8;
}

int trtoss(void) // 9170: throw
{
    int i;
    if (toting(game.rod2) && game.obj == game.rod && !toting(game.rod))
        game.obj = game.rod2;
    if (!toting(game.obj))
        return (2011);
    if (game.obj >= 50 && game.obj <= game.maxtrs && at(game.troll)) {
        game.spk = 159; // 9178
        drop(game.obj, 0);
        move(game.troll, 0);
        move(game.troll + 100, 0);
        drop(game.troll2, game.plac[game.troll]);
        drop(game.troll2 + 100, game.fixd[game.troll]);
        juggle(game.chasm);
        return (2011);
    }
    if (game.obj == game.food && here(game.bear)) {
        game.obj = game.bear; // 9177
        return (9210);
    }
    if (game.obj != game.axe)
        return (9020);
    for (i = 1; i <= 5; i++) {
        if (game.dloc[i] == game.loc) {
            game.spk = 48; // 9172
            if (ran(3) == 0 || game.saved != -1)
            l9175: {
                rspeak(game.spk);
                drop(game.axe, game.loc);
                game.k = game.null;
                return (8);
            }
                game.dseen[i] = FALSE;
            game.dloc[i] = 0;
            game.spk     = 47;
            game.dkill++;
            if (game.dkill == 1)
                game.spk = 149;
            goto l9175;
        }
    }
    game.spk = 152;
    if (at(game.dragon) && game.prop[game.dragon] == 0)
        goto l9175;
    game.spk = 158;
    if (at(game.troll))
        goto l9175;
    if (here(game.bear) && game.prop[game.bear] == 0) {
        game.spk = 164;
        drop(game.axe, game.loc);
        game.fixed[game.axe] = -1;
        game.prop[game.axe]  = 1;
        juggle(game.bear);
        return (2011);
    }
    game.obj = 0;
    return (9120);
}

int trfeed(void) // 9210
{
    if (game.obj == game.bird) {
        game.spk = 100;
        return (2011);
    }
    if (game.obj == game.snake || game.obj == game.dragon || game.obj == game.troll) {
        game.spk = 102;
        if (game.obj == game.dragon && game.prop[game.dragon] != 0)
            game.spk = 110;
        if (game.obj == game.troll)
            game.spk = 182;
        if (game.obj != game.snake || game.closed || !here(game.bird))
            return (2011);
        game.spk = 101;
        dstroy(game.bird);
        game.prop[game.bird] = 0;
        game.tally2++;
        return (2011);
    }
    if (game.obj == game.dwarf) {
        if (!here(game.food))
            return (2011);
        game.spk = 103;
        game.dflag++;
        return (2011);
    }
    if (game.obj == game.bear) {
        if (game.prop[game.bear] == 0)
            game.spk = 102;
        if (game.prop[game.bear] == 3)
            game.spk = 110;
        if (!here(game.food))
            return (2011);
        dstroy(game.food);
        game.prop[game.bear] = 1;
        game.fixed[game.axe] = 0;
        game.prop[game.axe]  = 0;
        game.spk             = 168;
        return (2011);
    }
    game.spk = 14;
    return (2011);
}

int trfill(void) // 9220
{
    if (game.obj == game.vase) {
        game.spk = 29;
        if (liqloc(game.loc) == 0)
            game.spk = 144;
        if (liqloc(game.loc) == 0 || !toting(game.vase))
            return (2011);
        rspeak(145);
        game.prop[game.vase]  = 2;
        game.fixed[game.vase] = -1;
        return (9020); // advent/10 goes to 9024
    }
    if (game.obj != 0 && game.obj != game.bottle)
        return (2011);
    if (game.obj == 0 && !here(game.bottle))
        return (8000);
    game.spk = 107;
    if (liqloc(game.loc) == 0)
        game.spk = 106;
    if (liq(0) != 0)
        game.spk = 105;
    if (game.spk != 107)
        return (2011);
    game.prop[game.bottle] = ((game.cond[game.loc] % 4) / 2) * 2;
    game.k                 = liq(0);
    if (toting(game.bottle))
        game.place[game.k] = -1;
    if (game.k == game.oil)
        game.spk = 108;
    return (2011);
}

int closing(void) // 10000
{
    int i;
    game.prop[game.grate] = game.prop[game.fissur] = 0;
    for (i = 1; i <= 6; i++) {
        game.dseen[i] = FALSE;
        game.dloc[i]  = 0;
    }
    move(game.troll, 0);
    move(game.troll + 100, 0);
    move(game.troll2, game.plac[game.troll]);
    move(game.troll2 + 100, game.fixd[game.troll]);
    juggle(game.chasm);
    if (game.prop[game.bear] != 3)
        dstroy(game.bear);
    game.prop[game.chain]  = 0;
    game.fixed[game.chain] = 0;
    game.prop[game.axe]    = 0;
    game.fixed[game.axe]   = 0;
    rspeak(129);
    game.clock1 = -1;
    game.closng = TRUE;
    return (19999);
}

int caveclose(void) // 11000
{
    int i;
    game.prop[game.bottle] = put(game.bottle, 115, 1);
    game.prop[game.plant]  = put(game.plant, 115, 0);
    game.prop[game.oyster] = put(game.oyster, 115, 0);
    game.prop[game.lamp]   = put(game.lamp, 115, 0);
    game.prop[game.rod]    = put(game.rod, 115, 0);
    game.prop[game.dwarf]  = put(game.dwarf, 115, 0);
    game.loc               = 115;
    game.oldloc            = 115;
    game.newloc            = 115;

    put(game.grate, 116, 0);
    game.prop[game.snake]  = put(game.snake, 116, 1);
    game.prop[game.bird]   = put(game.bird, 116, 1);
    game.prop[game.cage]   = put(game.cage, 116, 0);
    game.prop[game.rod2]   = put(game.rod2, 116, 0);
    game.prop[game.pillow] = put(game.pillow, 116, 0);

    game.prop[game.mirror]  = put(game.mirror, 115, 0);
    game.fixed[game.mirror] = 116;

    for (i = 1; i <= 100; i++)
        if (toting(i))
            dstroy(i);
    rspeak(132);
    game.closed = TRUE;
    return (2);
}

// The main program.  Upstream's main(), less the mode that compiled glorkz
// into adventure.dat: the data is in the binary and parsed at startup.
static Task<i32> adventure(Args args)
{
    int i;
    int rval;
    struct text *kk;
    Str savfile;

    if (args.size() > 1)
        savfile = args[1];

    rdata(); // read glorkz, build the tables
    linkdata();
    poof();

    if (!savfile.empty()) {
        Result<void> r = Err(Error::NotFound);
        if (Task<Result<void>> t = restore(savfile))
            r = co_await t;
        if (r.is_ok()) {
            if (co_await start(0) == ADV_OVER) // restarting game : 8305
                co_return adv_status;
            game.k = game.null;
            goto l8;
        }
        adv_printf("Your forged file dissappears in a puff of greasy black smoke! (poof)\n");
        if (Task<Result<void>> t = remove_path(savfile, false))
            co_await t;
        co_return 0;
    }
    if (co_await start(0) == ADV_OVER)
        co_return adv_status;
    if (co_await startup() == ADV_OVER) // prepare for a user
        co_return adv_status;
    game.blklin = TRUE;

    for (;;) // main command loop (label 2)
    {
        if (game.newloc < 9 && game.newloc != 0 && game.closng) {
            rspeak(130);            // if closing leave only by
            game.newloc = game.loc; // main office
            if (!game.panic)
                game.clock2 = 15;
            game.panic = TRUE;
        }

        rval = fdwarf(); // dwarf stuff
        if (rval == 99) {
            if (co_await die(99) == ADV_OVER)
                co_return adv_status;
        }

    l2000:
        if (game.loc == 0) { // label 2000
            if (co_await die(99) == ADV_OVER)
                co_return adv_status;
        }
        kk = &game.stext[game.loc];
        if ((game.abb[game.loc] % game.abbnum) == 0 || kk->seekadr == 0)
            kk = &game.ltext[game.loc];
        if (!forced(game.loc) && dark(0)) {
            if (game.wzdark && pct(35)) {
                if (co_await die(90) == ADV_OVER)
                    co_return adv_status;
                goto l2000;
            }
            kk = &game.rtext[16];
        }
        if (toting(game.bear))
            rspeak(141); // 2001
        speak(kk);
        game.k = 1;
        if (forced(game.loc))
            goto l8;
        if (game.loc == 33 && pct(25) && !game.closng)
            rspeak(8);
        if (!dark(0)) {
            game.abb[game.loc]++;
            for (i = game.atloc[game.loc]; i != 0; i = game.plink[i]) // 2004
            {
                game.obj = i;
                if (game.obj > 100)
                    game.obj -= 100;
                if (game.obj == game.steps && toting(game.nugget))
                    continue;
                if (game.prop[game.obj] < 0) {
                    if (game.closed)
                        continue;
                    game.prop[game.obj] = 0;
                    if (game.obj == game.rug || game.obj == game.chain)
                        game.prop[game.obj] = 1;
                    game.tally--;
                    if (game.tally == game.tally2 && game.tally != 0)
                        if (game.limit > 35)
                            game.limit = 35;
                }
                {
                    int pk = game.prop[game.obj]; // 2006
                    if (game.obj == game.steps && game.loc == game.fixed[game.steps])
                        pk = 1;
                    pspeak(game.obj, pk);
                }
            } // 2008
            goto l2012;
        l2009:
            game.k = 54; // 2009
        l2010:
            game.spk = game.k;
        l2011:
            rspeak(game.spk);
        }
    l2012:
        game.verb = 0; // 2012
        game.obj  = 0;
    l2600:
        if (Task<void> t = checkhints()) // to 2600-2602
            co_await t;
        if (game.closed) {
            if (game.prop[game.oyster] < 0 && toting(game.oyster))
                pspeak(game.oyster, 1);
            for (i = 1; i < 100; i++)
                if (toting(i) && game.prop[i] < 0) // 2604
                    game.prop[i] = -1 - game.prop[i];
        }
        game.wzdark = dark(0); // 2605
        if (game.knfloc > 0 && game.knfloc != game.loc)
            game.knfloc = 1;
        if (Task<void> t = getin(&wd1, &wd2))
            co_await t;
        if (delhit) // user typed a DEL
        {
            delhit = 0;           // reset counter
            copystr("quit", wd1); // pretend he's quitting
            *wd2 = 0;
        }
    l2608:
        if ((game.foobar = -game.foobar) > 0)
            game.foobar = 0; // 2608
        // should check here for "magic mode"
        game.turns++;

        if (game.verb == game.say && *wd2 != 0)
            game.verb = 0;
        if (game.verb == game.say)
            goto l4090;
        if (game.tally == 0 && game.loc >= 15 && game.loc != 33)
            game.clock1--;
        if (game.clock1 == 0) {
            closing(); // to 10000
            goto l19999;
        }
        if (game.clock1 < 0)
            game.clock2--;
        if (game.clock2 == 0) {
            caveclose(); // to 11000
            continue;    // back to 2
        }
        if (game.prop[game.lamp] == 1)
            game.limit--;
        if (game.limit <= 30 && here(game.batter) && game.prop[game.batter] == 0 &&
            here(game.lamp)) {
            rspeak(188); // 12000
            game.prop[game.batter] = 1;
            if (toting(game.batter))
                drop(game.batter, game.loc);
            game.limit  = game.limit + 2500;
            game.lmwarn = FALSE;
            goto l19999;
        }
        if (game.limit == 0) {
            game.limit           = -1; // 12400
            game.prop[game.lamp] = 0;
            rspeak(184);
            goto l19999;
        }
        if (game.limit < 0 && game.loc <= 8) {
            rspeak(185); // 12600
            game.gaveup = TRUE;
            co_await done(2); // to 20000
            co_return adv_status;
        }
        if (game.limit <= 30) {
            if (game.lmwarn || !here(game.lamp))
                goto l19999; // 12200
            game.lmwarn = TRUE;
            game.spk    = 187;
            if (game.place[game.batter] == 0)
                game.spk = 183;
            if (game.prop[game.batter] == 1)
                game.spk = 189;
            rspeak(game.spk);
        }
    l19999:
        game.k = 43;
        if (liqloc(game.loc) == game.water)
            game.k = 70;
        if (weq(wd1, "enter") && (weq(wd2, "strea") || weq(wd2, "water")))
            goto l2010;
        if (weq(wd1, "enter") && *wd2 != 0)
            goto l2800;
        if ((!weq(wd1, "water") && !weq(wd1, "oil")) || (!weq(wd2, "plant") && !weq(wd2, "door")))
            goto l2610;
        if (at(vocab(wd2, 1, 0)))
            copystr("pour", wd2);
    l2610:
        if (weq(wd1, "west"))
            if (++game.iwest == 10)
                rspeak(17);
    l2630:
        i = vocab(wd1, -1, 0);
        if (i == -1) {
            game.spk = 60; // 3000
            if (pct(20))
                game.spk = 61;
            if (pct(20))
                game.spk = 13;
            rspeak(game.spk);
            goto l2600;
        }
        game.k  = i % 1000;
        game.kq = i / 1000 + 1;
        switch (game.kq) {
        case 1:
            goto l8;
        case 2:
            goto l5000;
        case 3:
            goto l4000;
        case 4:
            goto l2010;
        default:
            adv_printf("Error 22\n");
            bug(22);
        }

    l8:
        switch (march()) {
        case 2:
            continue; // i.e. goto l2
        case 99:
            rval = co_await die(99);
            switch (rval) {
            case 2000:
                goto l2000;
            case ADV_OVER:
                co_return adv_status;
            default:
                bug(111);
            }
        default:
            bug(110);
        }

    l2800:
        copystr(wd2, wd1);
        *wd2 = 0;
        goto l2610;

    l4000:
        game.verb = game.k;
        game.spk  = game.actspk[game.verb];
        if (*wd2 != 0 && game.verb != game.say)
            goto l2800;
        if (game.verb == game.say)
            game.obj = *wd2;
        if (game.obj != 0)
            goto l4090;

        switch (game.verb) {
        case 1: // take = 8010
            if (game.atloc[game.loc] == 0 || game.plink[game.atloc[game.loc]] != 0)
                goto l8000;
            for (i = 1; i <= 5; i++)
                if (game.dloc[i] == game.loc && game.dflag >= 2)
                    goto l8000;
            game.obj = game.atloc[game.loc];
            goto l9010;
        case 2:
        case 3:
        case 9: // 8000 : drop,say,wave
        case 10:
        case 16:
        case 17: // calm,rub,toss
        case 19:
        case 21:
        case 28: // find,feed,break
        case 29: // wake
        l8000:
            adv_printf("%s what?\n", wd1);
            game.obj = 0;
            goto l2600;
        case 4:
        case 6: // 8040 open,lock
            game.spk = 28;
            if (here(game.clam))
                game.obj = game.clam;
            if (here(game.oyster))
                game.obj = game.oyster;
            if (at(game.door))
                game.obj = game.door;
            if (at(game.grate))
                game.obj = game.grate;
            if (game.obj != 0 && here(game.chain))
                goto l8000;
            if (here(game.chain))
                game.obj = game.chain;
            if (game.obj == 0)
                goto l2011;
            goto l9040;
        case 5:
            goto l2009; // nothing
        case 7:
            goto l9070; // on
        case 8:
            goto l9080; // off
        case 11:
            goto l8000; // walk
        case 12:
            goto l9120; // kill
        case 13:
            goto l9130; // pour
        case 14:        // eat: 8140
            if (!here(game.food))
                goto l8000;
        l8142:
            dstroy(game.food);
            game.spk = 72;
            goto l2011;
        case 15:
            goto l9150; // drink
        case 18:        // quit: 8180
            game.gaveup = co_await yes(22, 54, 54);
            if (game.gaveup) {
                co_await done(2); // 8185
                co_return adv_status;
            }
            goto l2012;
        case 20: // invent=8200
            game.spk = 98;
            for (i = 1; i <= 100; i++) {
                if (i != game.bear && toting(i)) {
                    if (game.spk == 98)
                        rspeak(99);
                    game.blklin = FALSE;
                    pspeak(i, -1);
                    game.blklin = TRUE;
                    game.spk    = 0;
                }
            }
            if (toting(game.bear))
                game.spk = 141;
            goto l2011;
        case 22:
            goto l9220; // fill
        case 23:
            goto l9230; // blast
        case 24:        // score: 8240
            game.scorng = TRUE;
            adv_printf("If you were to quit now, you would score");
            adv_printf(" %d out of a possible ", score());
            adv_printf("%d.", game.mxscor);
            game.scorng = FALSE;
            game.gaveup = co_await yes(143, 54, 54);
            if (game.gaveup) {
                co_await done(2);
                co_return adv_status;
            }
            goto l2012;
        case 25: // foo: 8250
            game.k   = vocab(wd1, 3, 0);
            game.spk = 42;
            if (game.foobar == 1 - game.k)
                goto l8252;
            if (game.foobar != 0)
                game.spk = 151;
            goto l2011;
        l8252:
            game.foobar = game.k;
            if (game.k != 4)
                goto l2009;
            game.foobar = 0;
            if (game.place[game.eggs] == game.plac[game.eggs] ||
                (toting(game.eggs) && game.loc == game.plac[game.eggs]))
                goto l2011;
            if (game.place[game.eggs] == 0 && game.place[game.troll] == 0 &&
                game.prop[game.troll] == 0)
                game.prop[game.troll] = 1;
            game.k = 2;
            if (here(game.eggs))
                game.k = 1;
            if (game.loc == game.plac[game.eggs])
                game.k = 0;
            move(game.eggs, game.plac[game.eggs]);
            pspeak(game.eggs, game.k);
            goto l2012;
        case 26: // brief=8260
            game.spk    = 156;
            game.abbnum = 10000;
            game.detail = 3;
            goto l2011;
        case 27: // read=8270
            if (here(game.magzin))
                game.obj = game.magzin;
            if (here(game.tablet))
                game.obj = game.obj * 100 + game.tablet;
            if (here(game.messag))
                game.obj = game.obj * 100 + game.messag;
            if (game.closed && toting(game.oyster))
                game.obj = game.oyster;
            if (game.obj > 100 || game.obj == 0 || dark(0))
                goto l8000;
            goto l9270;
        case 30: // suspend=8300
            game.spk = 201;
            adv_printf("I can suspend your adventure for you so");
            adv_printf(" you can resume later, but\n");
            adv_printf("you will have to wait at least");
            adv_printf(" %d minutes before continuing.", game.latncy);
            if (!co_await yes(200, 54, 54))
                goto l2012;
            if (Task<void> t = adv_clock())
                co_await t;
            datime(&game.saved, &game.savet);
            if (co_await ciao() == ADV_OVER)
                co_return adv_status;
            continue;
        case 31: // hours=8310
            adv_printf("Colossal cave is closed 9am-5pm Mon ");
            adv_printf("through Fri except holidays.\n");
            goto l2012;
        default:
            bug(23);
        }

    l4090:
        switch (game.verb) {
        case 1: // take = 9010
        l9010:
            switch (trtake()) {
            case 2011:
                goto l2011;
            case 9220:
                goto l9220;
            case 2009:
                goto l2009;
            case 2012:
                goto l2012;
            default:
                bug(102);
            }
        l9020:
        case 2: // drop = 9020
            switch (trdrop()) {
            case 2011:
                goto l2011;
            case 19000:
                co_await done(3);
                co_return adv_status;
            case 2012:
                goto l2012;
            default:
                bug(105);
            }
        case 3:
            switch (trsay()) {
            case 2012:
                goto l2012;
            case 2630:
                goto l2630;
            default:
                bug(107);
            }
        l9040:
        case 4:
        case 6: // open, close
            switch (tropen()) {
            case 2011:
                goto l2011;
            case 2010:
                goto l2010;
            default:
                bug(106);
            }
        case 5:
            goto l2009; // nothing
        case 7:         // on   9070
        l9070:
            if (!here(game.lamp))
                goto l2011;
            game.spk = 184;
            if (game.limit < 0)
                goto l2011;
            game.prop[game.lamp] = 1;
            rspeak(39);
            if (game.wzdark)
                goto l2000;
            goto l2012;

        case 8: // off
        l9080:
            if (!here(game.lamp))
                goto l2011;
            game.prop[game.lamp] = 0;
            rspeak(40);
            if (dark(0))
                rspeak(16);
            goto l2012;

        case 9: // wave
            if ((!toting(game.obj)) && (game.obj != game.rod || !toting(game.rod2)))
                game.spk = 29;
            if (game.obj != game.rod || !at(game.fissur) || !toting(game.obj) || game.closng)
                goto l2011;
            game.prop[game.fissur] = 1 - game.prop[game.fissur];
            pspeak(game.fissur, 2 - game.prop[game.fissur]);
            goto l2012;
        case 10:
        case 11:
        case 18: // calm, walk, quit
        case 24:
        case 25:
        case 26: // score, foo, brief
        case 30:
        case 31: // suspend, hours
            goto l2011;
        l9120:
        case 12: // kill
            rval = co_await trkill();
            switch (rval) {
            case 8000:
                goto l8000;
            case 8:
                goto l8;
            case 2011:
                goto l2011;
            case 2608:
                goto l2608;
            case 19000:
                co_await done(3);
                co_return adv_status;
            default:
                bug(112);
            }
        l9130:
        case 13: // pour
            if (game.obj == game.bottle || game.obj == 0)
                game.obj = liq(0);
            if (game.obj == 0)
                goto l8000;
            if (!toting(game.obj))
                goto l2011;
            game.spk = 78;
            if (game.obj != game.oil && game.obj != game.water)
                goto l2011;
            game.prop[game.bottle] = 1;
            game.place[game.obj]   = 0;
            game.spk               = 77;
            if (!(at(game.plant) || at(game.door)))
                goto l2011;
            if (at(game.door)) {
                game.prop[game.door] = 0; // 9132
                if (game.obj == game.oil)
                    game.prop[game.door] = 1;
                game.spk = 113 + game.prop[game.door];
                goto l2011;
            }
            game.spk = 112;
            if (game.obj != game.water)
                goto l2011;
            pspeak(game.plant, game.prop[game.plant] + 1);
            game.prop[game.plant]  = (game.prop[game.plant] + 2) % 6;
            game.prop[game.plant2] = game.prop[game.plant] / 2;
            game.k                 = game.null;
            goto l8;
        case 14: // 9140 - eat
            if (game.obj == game.food)
                goto l8142;
            if (game.obj == game.bird || game.obj == game.snake || game.obj == game.clam ||
                game.obj == game.oyster || game.obj == game.dwarf || game.obj == game.dragon ||
                game.obj == game.troll || game.obj == game.bear)
                game.spk = 71;
            goto l2011;
        l9150:
        case 15: // 9150 - drink
            if (game.obj == 0 && liqloc(game.loc) != game.water &&
                (liq(0) != game.water || !here(game.bottle)))
                goto l8000;
            if (game.obj != 0 && game.obj != game.water)
                game.spk = 110;
            if (game.spk == 110 || liq(0) != game.water || !here(game.bottle))
                goto l2011;
            game.prop[game.bottle] = 1;
            game.place[game.water] = 0;
            game.spk               = 74;
            goto l2011;
        case 16: // 9160: rub
            if (game.obj != game.lamp)
                game.spk = 76;
            goto l2011;
        case 17: // 9170: throw
            switch (trtoss()) {
            case 2011:
                goto l2011;
            case 9020:
                goto l9020;
            case 9120:
                goto l9120;
            case 8:
                goto l8;
            case 9210:
                goto l9210;
            default:
                bug(113);
            }
        case 19:
        case 20: // 9190: find, invent
            if (at(game.obj) || (liq(0) == game.obj && at(game.bottle)) ||
                game.k == liqloc(game.loc))
                game.spk = 94;
            for (i = 1; i <= 5; i++)
                if (game.dloc[i] == game.loc && game.dflag >= 2 && game.obj == game.dwarf)
                    game.spk = 94;
            if (game.closed)
                game.spk = 138;
            if (toting(game.obj))
                game.spk = 24;
            goto l2011;
        l9210:
        case 21: // feed
            switch (trfeed()) {
            case 2011:
                goto l2011;
            default:
                bug(114);
            }
        l9220:
        case 22: // fill
            switch (trfill()) {
            case 2011:
                goto l2011;
            case 8000:
                goto l8000;
            case 9020:
                goto l9020;
            default:
                bug(115);
            }
        l9230:
        case 23: // blast
            if (game.prop[game.rod2] < 0 || !game.closed)
                goto l2011;
            game.bonus = 133;
            if (game.loc == 115)
                game.bonus = 134;
            if (here(game.rod2))
                game.bonus = 135;
            rspeak(game.bonus);
            co_await done(2);
            co_return adv_status;
        l9270:
        case 27: // read
            if (dark(0))
                goto l5190;
            if (game.obj == game.magzin)
                game.spk = 190;
            if (game.obj == game.tablet)
                game.spk = 196;
            if (game.obj == game.messag)
                game.spk = 191;
            if (game.obj == game.oyster && game.hinted[2] && toting(game.oyster))
                game.spk = 194;
            if (game.obj != game.oyster || game.hinted[2] || !toting(game.oyster) || !game.closed)
                goto l2011;
            game.hinted[2] = co_await yes(192, 193, 54);
            goto l2012;
        case 28: // break
            if (game.obj == game.mirror)
                game.spk = 148;
            if (game.obj == game.vase && game.prop[game.vase] == 0) {
                game.spk = 198;
                if (toting(game.vase))
                    drop(game.vase, game.loc);
                game.prop[game.vase]  = 2;
                game.fixed[game.vase] = -1;
                goto l2011;
            }
            if (game.obj != game.mirror || !game.closed)
                goto l2011;
            rspeak(197);
            co_await done(3);
            co_return adv_status;

        case 29: // wake
            if (game.obj != game.dwarf || !game.closed)
                goto l2011;
            rspeak(199);
            co_await done(3);
            co_return adv_status;

        default:
            bug(24);
        }

    l5000:
        game.obj = game.k;
        if (game.fixed[game.k] != game.loc && !here(game.k))
            goto l5100;
    l5010:
        if (*wd2 != 0)
            goto l2800;
        if (game.verb != 0)
            goto l4090;
        adv_printf("What do you want to do with the %s?\n", wd1);
        goto l2600;
    l5100:
        if (game.k != game.grate)
            goto l5110;
        if (game.loc == 1 || game.loc == 4 || game.loc == 7)
            game.k = game.dprssn;
        if (game.loc > 9 && game.loc < 15)
            game.k = game.entrnc;
        if (game.k != game.grate)
            goto l8;
    l5110:
        if (game.k != game.dwarf)
            goto l5120;
        for (i = 1; i <= 5; i++)
            if (game.dloc[i] == game.loc && game.dflag >= 2)
                goto l5010;
    l5120:
        if ((liq(0) == game.k && here(game.bottle)) || game.k == liqloc(game.loc))
            goto l5010;
        if (game.obj != game.plant || !at(game.plant2) || game.prop[game.plant2] == 0)
            goto l5130;
        game.obj = game.plant2;
        goto l5010;
    l5130:
        if (game.obj != game.knife || game.knfloc != game.loc)
            goto l5140;
        game.knfloc = -1;
        game.spk    = 116;
        goto l2011;
    l5140:
        if (game.obj != game.rod || !here(game.rod2))
            goto l5190;
        game.obj = game.rod2;
        goto l5010;
    l5190:
        if ((game.verb == game.find || game.verb == game.invent) && *wd2 == 0)
            goto l5010;
        adv_printf("I see no %s here\n", wd1);
        goto l2012;
    }
}

Task<i32> proc_main(Args args)
{
    if (Task<void> t = adv_input_init())
        co_await t;

    i32 rc = 0;
    if (Task<i32> t = adventure(args))
        rc = co_await t;
    else
        rc = 1;

    if (Task<void> t = adv_flush())
        co_await t;
    if (Task<void> t = adv_input_done())
        co_await t;
    co_return adv_write_failed() ? 1 : rc;
}
