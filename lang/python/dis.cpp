// --dis: the code object as a listing, and every code object in its constants
// after it.
//
// The format is this implementation's own -- there is no CPython to measure a
// private bytecode against -- so the goldens under test/dis/ are blessed from
// what this prints, and they are there to catch a change nobody meant.
#include "code.h"
#include "err.h"
#include "kernel/fmt.h"
#include "ops.h"

namespace {

struct Lister {
    String *out;
    bool ok = true;

    void put(Str s)
    {
        if (ok && !out->append(s))
            ok = false;
    }

    void put(char c)
    {
        if (ok && !out->push(c))
            ok = false;
    }

    void num(u64 v)
    {
        Buf<24> b;
        b.put(v);
        put(b.str());
    }

    void field(Str s, usize width, bool right)
    {
        usize n = s.size() < width ? width - s.size() : 0;
        if (right)
            for (usize k = 0; k < n; k++)
                put(' ');
        put(s);
        if (!right)
            for (usize k = 0; k < n; k++)
                put(' ');
    }

    void number(u64 v, usize width, bool right)
    {
        Buf<24> b;
        b.put(v);
        field(b.str(), width, right);
    }

    void names(Str label, const Vec<Value> &v)
    {
        if (!v.size())
            return;
        put("  ");
        put(label);
        put(':');
        for (usize k = 0; k < v.size(); k++) {
            put(k ? ", " : " ");
            put(is_str(v[k]) ? str_of(v[k])->str() : Str("?"));
        }
        put('\n');
    }

    void flags(u32 f)
    {
        constexpr Str NAME[] = { "varargs", "varkw", "generator", "newlocals", "nested" };
        bool first           = true;
        for (u32 b = 0; b < 5; b++) {
            if (!(f & (1u << b)))
                continue;
            put(' ');
            put(NAME[b]);
            first = false;
        }
        if (first)
            put(" -");
    }

    void hint(const CodeObj *c, const Instr &in);
    void one(const CodeObj *c);
};

void Lister::hint(const CodeObj *c, const Instr &in)
{
    switch (bc_arg(in.op)) {
    case Arg::Const:
        if (in.arg < c->consts.size()) {
            put(" (");
            if (py_repr(c->consts[in.arg], *out) != R::Ok)
                ok = false;
            put(')');
        }
        return;
    case Arg::Name:
        if (in.arg < c->names.size()) {
            put(" (");
            put(str_of(c->names[in.arg])->str());
            put(')');
        }
        return;
    case Arg::Local:
        if (in.arg < c->varnames.size()) {
            put(" (");
            put(str_of(c->varnames[in.arg])->str());
            put(')');
        }
        return;
    case Arg::Deref: {
        usize nc            = c->cellvars.size();
        const Vec<Value> &v = in.arg < nc ? c->cellvars : c->freevars;
        usize k             = in.arg < nc ? in.arg : in.arg - nc;
        if (k < v.size()) {
            put(" (");
            put(str_of(v[k])->str());
            put(')');
        }
        return;
    }
    case Arg::Un: {
        constexpr Str UN[] = { "~", "not", "+", "-" };
        put(" (");
        put(UN[in.arg & 3]);
        put(')');
        return;
    }
    case Arg::Bin:
        put(" (");
        put(op_symbol(Op(in.arg)));
        put(')');
        return;
    case Arg::Cmp:
        put(" (");
        put(cmp_symbol(Cmp(in.arg)));
        put(')');
        return;
    case Arg::Flags:
        if (in.op == Bc::MakeFunction) {
            if (!in.arg)
                return;
            put(" (");
            bool first = true;
            if (in.arg & MF_DEFAULTS) {
                put("defaults");
                first = false;
            }
            if (in.arg & MF_KWDEFAULTS) {
                put(first ? "" : " ");
                put("kwdefaults");
                first = false;
            }
            if (in.arg & MF_CLOSURE) {
                put(first ? "" : " ");
                put("closure");
            }
            put(')');
        } else if (in.arg & CX_KWARGS) {
            put(" (kwargs)");
        }
        return;
    case Arg::Jump:
    case Arg::Num:
    case Arg::None:
        return;
    }
}

void Lister::one(const CodeObj *c)
{
    put("code ");
    put(is_str(c->name) ? str_of(c->name)->str() : Str("?"));
    put("  file ");
    put(is_str(c->filename) ? str_of(c->filename)->str() : Str("?"));
    put("  line ");
    num(c->firstline);
    put('\n');

    put("  args ");
    num(c->argcount);
    put(" (posonly ");
    num(c->posonly);
    put(", kwonly ");
    num(c->kwonly);
    put(")  stack ");
    num(c->stacksize);
    put("  flags");
    flags(c->flags);
    put('\n');

    names("varnames", c->varnames);
    names("cellvars", c->cellvars);
    names("freevars", c->freevars);
    names("names", c->names);

    if (c->consts.size()) {
        put("  consts:\n");
        for (usize k = 0; k < c->consts.size(); k++) {
            put("    ");
            number(k, 3, true);
            put("  ");
            if (py_repr(c->consts[k], *out) != R::Ok)
                ok = false;
            put('\n');
        }
    }

    // A jump target is marked rather than named, so a listing reads top down.
    Vec<bool> target;
    if (!target.resize(c->code.size())) {
        ok = false;
        return;
    }
    for (usize k = 0; k < c->code.size(); k++)
        if (bc_arg(c->code[k].op) == Arg::Jump && c->code[k].arg < target.size())
            target[c->code[k].arg] = true;

    u32 line = 0;
    for (usize k = 0; k < c->code.size(); k++) {
        u32 at = code_line(c, u32(k));
        if (at != line) {
            line = at;
            number(line, 5, true);
        } else {
            field(Str(), 5, true);
        }
        put(target[k] ? " >> " : "    ");
        number(k, 4, true);
        put("  ");
        if (bc_arg(c->code[k].op) == Arg::None) {
            put(bc_name(c->code[k].op));
        } else {
            field(bc_name(c->code[k].op), 18, false);
            number(c->code[k].arg, 5, true);
            hint(c, c->code[k]);
        }
        put('\n');
    }
}

} // namespace

bool code_dis(const CodeObj *c, String &out)
{
    Lister l{ &out };
    l.one(c);
    for (usize k = 0; k < c->consts.size() && l.ok; k++)
        if (is_code(c->consts[k])) {
            l.put("\n");
            if (!code_dis(code_of(c->consts[k]), out))
                return false;
        }
    return l.ok;
}
