// PTRGET: resolving a variable name, and the arrays behind it.
//
// m6502.asm:3600-4234; 07-variables-arrays.md.
#include "mbasic.h"

// ============================================================== PTRGET
//
// Returns where the variable's value lives, leaving TXTPTR on the terminating
// character, and creates the variable if it does not exist -- usually.
//
// The name is two bytes with the type in their high bits (mbasic.h). Setting a
// bit on the name rather than storing a type tag is what keeps A and A$
// distinct in a single flat search: they simply have different two-byte names
// and never match each other. Only the first two characters are significant --
// EATEM discards the rest -- so COUNT and COUNTER are the same variable.
//
// `create` is upstream's NOTFNS, which inspected its own caller's return
// address to see whether it had been called from ISVAR. If so the variable is
// being READ in an expression and is not created; the answer is a pointer to
// ZERO, a block of zero bytes that reads correctly as 0.0, as integer 0 and as
// the null string. So PRINT X for an unknown X prints 0 without consuming any
// memory, while X=0 creates the entry.
VarRef Interp::ptrget(bool create)
{
    VarRef r;
    if (pending()) // poisoned (err.h): never a live variable
        return VarRef::scratch();

    u8 c = chrgot();
    if (!isletc(c))
        ERRV(VarRef::scratch(), ERRSN);

    u8 n0 = c;
    u8 n1 = 0;
    c     = chrget();
    if (isletc(c) || isdigit(c)) {
        n1 = c;
        c  = chrget();
    }
    while (isletc(c) || isdigit(c)) // EATEM
        c = chrget();

    valtyp = VNUM;
    intflg = false;
    if (c == '$') {
        valtyp = VSTR;
        n1 |= 0x80;
        c = chrget();
    } else if (c == '%') {
        // % is rejected when SUBFLG is set: integers cannot be FOR variables,
        // DEF FN names, or DEF FN parameters.
        if (subflg)
            ERRV(VarRef::scratch(), ERRSN);
        intflg = true;
        n0 |= 0x80;
        n1 |= 0x80;
        c = chrget();
    }

    // STRNAM's `ORA SUBFLG / SBCI 40` is the whole mechanism for forbidding
    // subscripts: with SUBFLG set the OR can never leave 40, so '(' is not
    // recognised as opening a subscript list and the stray paren becomes a
    // syntax error one level up.
    bool want_array = (c == '(' && subflg == 0);
    subflg          = 0;

    varnam[0] = n0;
    varnam[1] = n1;

    if (want_array)
        return isary(create);

    for (usize i = 0; i < vars.size(); i++)
        if (vars[i].name[0] == n0 && vars[i].name[1] == n1) {
            r.kind   = VarRef::Simple;
            r.index  = u32(i);
            r.valtyp = valtyp;
            r.intflg = intflg;
            return r;
        }

    if (!create) {
        r        = VarRef::scratch();
        r.valtyp = valtyp;
        r.intflg = intflg;
        return r;
    }

    Var v;
    v.name[0] = n0;
    v.name[1] = n1;
    v.type    = intflg ? VType::Int : (valtyp == VSTR ? VType::Str : VType::Num);
    if (!reason(vars.push(static_cast<Var &&>(v))))
        return VarRef::scratch();

    r.kind   = VarRef::Simple;
    r.index  = u32(vars.size() - 1);
    r.valtyp = valtyp;
    r.intflg = intflg;
    return r;
}

// ============================================================== arrays
//
// The layout upstream documented at m6502.asm:3810-3820 was a header, a byte
// count, a dimension count and then the extents -- stored big-endian and in
// REVERSE subscript order, because ISARY collected subscripts onto the 6502
// stack and the creation loop popped them. The combined effect is that for
// A(i,j) the linear element index is j*extent_i + i: the FIRST subscript
// varies fastest, i.e. column major. The extents are in written order here and
// the indexing below does the same arithmetic.
VarRef Interp::isary(bool create)
{
    u8 n0 = varnam[0], n1 = varnam[1];
    u8 vt    = valtyp;
    bool ifl = intflg;
    u8 dim   = dimflg;

    Vec<u16> subs;
    do {
        chrget();
        // INTIDX forces a non-negative integer through POSINT, so A(-1) gives
        // ?ILLEGAL QUANTITY and not ?BAD SUBSCRIPT.
        i16 s = posint();
        CHKV(VarRef::scratch());
        if (!reason(subs.push(u16(s))))
            return VarRef::scratch();
        // The flags have to be restored after every subscript: INTIDX calls
        // FRMEVL, which can recurse all the way back into PTRGET for a
        // subscript like A(B(I)).
        valtyp    = vt;
        intflg    = ifl;
        dimflg    = dim;
        varnam[0] = n0;
        varnam[1] = n1;
    } while (chrgot() == ',');

    if (chrgot() != ')')
        ERRV(VarRef::scratch(), ERRSN);
    chrget();
    dimflg = 0;

    VarRef r;
    for (usize i = 0; i < arrays.size(); i++) {
        Array &a = arrays[i];
        if (a.name[0] != n0 || a.name[1] != n1)
            continue;
        if (dim)
            ERRV(VarRef::scratch(), ERRDD); // ?REDIM'D ARRAY
        // Using the wrong NUMBER of subscripts and using one out of range both
        // give ?BS.
        if (subs.size() != a.extent.size())
            ERRV(VarRef::scratch(), ERRBS);
        r.kind   = VarRef::Element;
        r.index  = u32(i);
        r.valtyp = vt;
        r.intflg = ifl;
        return element_of(r, subs);
    }

    // NOTFDD. An undimensioned array gets an extent of 11 per dimension, so
    // subscripts run 0..10; indexing is 0-based throughout and there is no
    // OPTION BASE.
    Array a;
    a.name[0] = n0;
    a.name[1] = n1;
    a.type    = ifl ? VType::Int : (vt == VSTR ? VType::Str : VType::Num);
    u32 total = 1;
    for (usize i = 0; i < subs.size(); i++) {
        u32 e = dim ? u32(subs[i]) + 1 : 11;
        if (e == 0 || total > 0xFFFF / (e ? e : 1))
            ERRV(VarRef::scratch(), ERROM); // UMULT's carry out was ?OM, not ?BS
        if (!reason(a.extent.push(u16(e))))
            return VarRef::scratch();
        total *= e;
    }

    // The element area is zeroed, which is why numeric arrays start at 0 and
    // string arrays start as null strings.
    bool ok = true;
    switch (a.type) {
    case VType::Num:
        ok = a.num.resize(total);
        for (u32 i = 0; ok && i < total; i++)
            a.num[i] = 0;
        break;
    case VType::Int:
        ok = a.ints.resize(total);
        for (u32 i = 0; ok && i < total; i++)
            a.ints[i] = 0;
        break;
    default:
        ok = a.str.resize(total);
        break;
    }
    if (!reason(ok) || !reason(arrays.push(static_cast<Array &&>(a))))
        return VarRef::scratch();

    r.kind   = VarRef::Element;
    r.index  = u32(arrays.size() - 1);
    r.valtyp = vt;
    r.intflg = ifl;
    if (dim) {
        r.elem = 0;
        return r; // DIM does not index
    }
    (void)create;
    return element_of(r, subs);
}

// GETDEF (m6502.asm:4016-4075). Subscripts were popped in the reverse of the
// order they were written and the extents were stored in that same reverse
// order, so the two lined up; written order here, same arithmetic.
VarRef Interp::element_of(VarRef r, const Vec<u16> &subs)
{
    const Array &a = arrays[r.index];
    u32 idx        = 0;
    for (usize k = subs.size(); k-- > 0;) {
        if (subs[k] >= a.extent[k])
            ERRV(VarRef::scratch(), ERRBS);
        idx = idx * a.extent[k] + subs[k];
    }
    r.elem = idx;
    return r;
}

// ============================================================== access

f64 Interp::var_get_num(VarRef v)
{
    switch (v.kind) {
    case VarRef::Simple:
        return v.intflg ? f64(vars[v.index].ival) : vars[v.index].num;
    case VarRef::Element: {
        const Array &a = arrays[v.index];
        return a.type == VType::Int ? f64(a.ints[v.elem]) : a.num[v.elem];
    }
    default:
        return 0; // ZERO
    }
}

void Interp::var_put_num(VarRef v, f64 x)
{
    switch (v.kind) {
    case VarRef::Simple:
        if (v.intflg)
            vars[v.index].ival = i16(x);
        else
            vars[v.index].num = x;
        return;
    case VarRef::Element: {
        Array &a = arrays[v.index];
        if (a.type == VType::Int)
            a.ints[v.elem] = i16(x);
        else
            a.num[v.elem] = x;
        return;
    }
    default:
        return;
    }
}

Str Interp::var_get_str(VarRef v)
{
    switch (v.kind) {
    case VarRef::Simple:
        return vars[v.index].str.str();
    case VarRef::Element:
        return arrays[v.index].str[v.elem].str();
    default:
        return Str();
    }
}

bool Interp::var_put_str(VarRef v, Str s)
{
    switch (v.kind) {
    case VarRef::Simple:
        return vars[v.index].str.assign(s);
    case VarRef::Element:
        return arrays[v.index].str[v.elem].assign(s);
    default:
        return true;
    }
}

// ============================================================== DIM
//
// DIMFLG bit 6 says "take the extents from the subscripts rather than the
// default of 11", and its being non-zero afterwards says "this was a DIM, so
// return without indexing".
void Interp::stmt_dim()
{
    for (;;) {
        dimflg = 0x40;
        ptrget();
        dimflg = 0;
        CHK;
        if (chrgot() != ',')
            return;
        chrget();
    }
}

// ============================================================== DEF FN
//
// m6502.asm:4134-4234. The source records the restrictions at 4136-4139: a
// single numeric argument and a single-line definition.
//
// Upstream stored the body's text pointer and a pointer to the argument
// VARIABLE, and made the parameter an ordinary variable saved and restored
// around the call -- which is the whole mechanism, and is why a user can see
// that FNA(3) leaves X unchanged while a body referencing FNA still recurses.
void Interp::stmt_def()
{
    VarRef fn = getfnm();
    CHK;
    // ERRDIR: definitions are illegal in direct mode, detected as CURLIN+1 == 255.
    if (curlin == DIRECT_LINE)
        ERR(ERRID);
    if (chrgot() != '(')
        ERR(ERRSN);
    chrget();

    subflg     = 128; // no subscripts and no % for the parameter
    VarRef arg = ptrget();
    CHK;
    if (valtyp == VSTR)
        ERR(ERRTM);
    if (chrgot() != ')')
        ERR(ERRSN);
    chrget();
    synchr(EQULTK);
    CHK;

    Var &v   = vars[fn.index];
    v.type   = VType::Fn;
    v.fnbody = txtptr;
    v.fnarg  = arg.index;
    v.fndef  = true;
    datan(); // skip to the end of the statement
}

// GETFNM (m6502.asm:4180-4185). Setting SUBFLG to the character with bit 7 set
// does double duty: it forbids subscripts and carries the first name character
// into PTRGT2, which is what puts the function bit on the name.
VarRef Interp::getfnm()
{
    synchr(FNTK);
    CHKV(VarRef::scratch());

    u8 c = chrgot();
    if (!isletc(c))
        ERRV(VarRef::scratch(), ERRSN);

    subflg = 128;
    u8 n0  = u8(c | 0x80);
    u8 n1  = 0;
    c      = chrget();
    if (isletc(c) || isdigit(c)) {
        n1 = c;
        c  = chrget();
    }
    while (isletc(c) || isdigit(c))
        c = chrget();
    subflg = 0;

    if (c == '$')
        ERRV(VarRef::scratch(), ERRTM); // function names may not be strings

    VarRef r;
    for (usize i = 0; i < vars.size(); i++)
        if (vars[i].name[0] == n0 && vars[i].name[1] == n1) {
            r.kind  = VarRef::Simple;
            r.index = u32(i);
            return r;
        }

    Var v;
    v.name[0] = n0;
    v.name[1] = n1;
    v.type    = VType::Fn;
    if (!reason(vars.push(static_cast<Var &&>(v))))
        return VarRef::scratch();
    r.kind  = VarRef::Simple;
    r.index = u32(vars.size() - 1);
    return r;
}

// FNDOER (m6502.asm:4186-4217). The argument variable's current value is
// pushed, the actual is stored into it, the body is evaluated, and the saved
// value is put back -- which is what makes recursion and nesting safe.
void Interp::fndoer()
{
    VarRef fn = getfnm();
    CHK;
    Var &f = vars[fn.index];
    if (!f.fndef) // never defined: PTRGT2 auto-created it with zero bytes
        ERR(ERRUF);

    u32 argi     = f.fnarg;
    TextPos body = f.fnbody;

    parchk(); // the parenthesised actual
    CHK;
    chknum();
    CHK;

    if (!getstk(1))
        return;
    Depth d(*this);

    f64 saved      = vars[argi].num;
    vars[argi].num = fac.n;
    TextPos back   = txtptr;
    txtptr         = body;
    frmnum();
    txtptr         = back;
    vars[argi].num = saved;
}
