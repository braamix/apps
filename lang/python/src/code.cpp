// The code object: allocation, tracing, the line table, and the two tables the
// opcode list generates.
#include "code.h"

#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"

namespace {

constexpr Str NAMES[] = {
#define BC_NAME(n, a) #n,
    BC_LIST(BC_NAME)
#undef BC_NAME
};

constexpr Arg ARGS[] = {
#define BC_ARG(n, a) Arg::a,
    BC_LIST(BC_ARG)
#undef BC_ARG
};

constexpr usize NOPS = sizeof(NAMES) / sizeof(NAMES[0]);

void mark_all(const Vec<Value> &v)
{
    for (usize i = 0; i < v.size(); i++)
        gc_mark(v[i]);
}

void code_trace(Obj *o)
{
    CodeObj *c = static_cast<CodeObj *>(o);
    mark_all(c->consts);
    mark_all(c->names);
    mark_all(c->varnames);
    mark_all(c->cellvars);
    mark_all(c->freevars);
    gc_mark(c->name);
    gc_mark(c->qualname);
    gc_mark(c->doc);
    gc_mark(c->filename);
}

void code_fini(Obj *o)
{
    CodeObj *c = static_cast<CodeObj *>(o);
    c->code.~Vec();
    c->consts.~Vec();
    c->names.~Vec();
    c->varnames.~Vec();
    c->cellvars.~Vec();
    c->freevars.~Vec();
    c->lines.~Vec();
}

R code_repr(Value v, String &out)
{
    CodeObj *c = code_of(v);
    Buf<96> b;
    b.put("<code ");
    b.put(is_str(c->name) ? str_of(c->name)->str() : Str("?"));
    b.put(" at line ").put(u64(c->firstline)).put('>');
    return out.append(b.str()) ? R::Ok : err_set("MemoryError", "out of memory");
}

// A name array as a tuple, which is what every co_* of names answers with.
Value names_tuple(const Vec<Value> &a, const Vec<Value> &b)
{
    TupleObj *t = tuple_new(a.size() + b.size());
    if (!t)
        return err_set("MemoryError", "out of memory"), Value();
    for (usize i = 0; i < a.size(); i++)
        t->items()[i] = a[i];
    for (usize i = 0; i < b.size(); i++)
        t->items()[a.size() + i] = b[i];
    return obj_value(t);
}

R code_getattr(Value v, StrObj *name, Value &out)
{
    CodeObj *c = code_of(v);
    Str n      = name->str();
    Vec<Value> none;
    if (n == "co_name")
        out = c->name;
    else if (n == "co_qualname")
        out = c->qualname;
    else if (n == "co_filename")
        out = c->filename;
    else if (n == "co_firstlineno")
        out = Value::of_int(i64(c->firstline));
    else if (n == "co_argcount")
        out = Value::of_int(i64(c->argcount));
    else if (n == "co_posonlyargcount")
        out = Value::of_int(i64(c->posonly));
    else if (n == "co_kwonlyargcount")
        out = Value::of_int(i64(c->kwonly));
    else if (n == "co_flags")
        out = Value::of_int(i64(c->flags));
    else if (n == "co_stacksize")
        out = Value::of_int(i64(c->stacksize));
    else if (n == "co_nlocals")
        out = Value::of_int(i64(c->varnames.size()));
    else if (n == "co_consts")
        out = names_tuple(c->consts, none);
    else if (n == "co_names")
        out = names_tuple(c->names, none);
    else if (n == "co_varnames")
        out = names_tuple(c->varnames, none);
    else if (n == "co_cellvars")
        out = names_tuple(c->cellvars, none);
    else if (n == "co_freevars")
        out = names_tuple(c->freevars, none);
    else
        return R::NotImpl;
    return out.is_nil() ? R::Err : R::Ok;
}

template <typename T>
bool copy_vec(Vec<T> &to, const Vec<T> &from)
{
    if (!to.resize(from.size()))
        return false;
    for (usize i = 0; i < from.size(); i++)
        to[i] = from[i];
    return true;
}

// The fields replace() may be given, by keyword.
enum : u32 { RP_FLAGS, RP_NAME, RP_QUALNAME, RP_FILENAME, RP_FIRSTLINE, RP_COUNT };

constexpr Str RP_NAMES[RP_COUNT] = { "co_flags", "co_name", "co_qualname", "co_filename",
                                     "co_firstlineno" };

// code.replace(**changes): a copy with some fields different. types.coroutine
// is the reason it exists, which sets one flag.
R m_replace(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || !is_code(a.args[0]))
        return err_set("TypeError", "replace() takes no positional arguments");
    Value got[RP_COUNT];
    Roots pin{ got, RP_COUNT };
    i64 num[RP_COUNT] = {};
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = str_of(a.kwnames[k])->str();
        u32 f = 0;
        while (f < RP_COUNT && RP_NAMES[f] != n)
            f++;
        if (f == RP_COUNT) {
            Buf<96> b;
            b.put("replace() got an unexpected keyword argument '").put(n).put("'");
            return err_set("TypeError", b.str());
        }
        bool number = f == RP_FLAGS || f == RP_FIRSTLINE;
        i64 x;
        if (number ? !as_index(a.kwvals[k], x) || x < 0 || x > 0x7fffffff : !is_str(a.kwvals[k]))
            return err_set2(
                "TypeError",
                number ? "replace() argument must be int" : "replace() argument must be str",
                RP_NAMES[f]);
        got[f] = a.kwvals[k];
        num[f] = number ? x : 0;
    }

    Root self{ a.args[0] };
    CodeObj *o = code_of(self.v);
    CodeObj *c = code_new(o->name, o->filename, o->firstline);
    if (!c)
        return err_set("MemoryError", "out of memory");
    Root made{ obj_value(c) };
    o = code_of(self.v);
    if (!copy_vec(c->code, o->code) || !copy_vec(c->consts, o->consts) ||
        !copy_vec(c->names, o->names) || !copy_vec(c->varnames, o->varnames) ||
        !copy_vec(c->cellvars, o->cellvars) || !copy_vec(c->freevars, o->freevars) ||
        !copy_vec(c->lines, o->lines))
        return err_set("MemoryError", "out of memory");
    c->qualname  = o->qualname;
    c->doc       = o->doc;
    c->flags     = o->flags;
    c->argcount  = o->argcount;
    c->posonly   = o->posonly;
    c->kwonly    = o->kwonly;
    c->stacksize = o->stacksize;
    c->nblocks   = o->nblocks;

    if (!got[RP_FLAGS].is_nil())
        c->flags = u32(num[RP_FLAGS]);
    if (!got[RP_FIRSTLINE].is_nil())
        c->firstline = u32(num[RP_FIRSTLINE]);
    if (!got[RP_NAME].is_nil())
        c->name = got[RP_NAME];
    if (!got[RP_QUALNAME].is_nil())
        c->qualname = got[RP_QUALNAME];
    if (!got[RP_FILENAME].is_nil())
        c->filename = got[RP_FILENAME];
    out = made.v;
    return R::Ok;
}

// An iterator over `n` tuples that `make` fills in, one per index.
R tuples_iter(u32 n, u32 width, void (*make)(const CodeObj *, u32 i, Value *out), Value self,
              Value &out)
{
    Root rs{ self };
    ListObj *l = list_new();
    if (!l)
        return err_set("MemoryError", "out of memory");
    Root rl{ obj_value(l) };
    for (u32 i = 0; i < n; i++) {
        TupleObj *t = tuple_new(width);
        if (!t || !list_push(list_of(rl.v), obj_value(t)))
            return err_set("MemoryError", "out of memory");
        make(code_of(rs.v), i, t->items());
    }
    out = py_iter(rl.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// co_positions(): (line, end line, column, end column) per instruction. The
// compiler keeps lines only, so the columns are None.
R m_co_positions(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || !is_code(a.args[0]))
        return err_set("TypeError", "co_positions() takes no arguments");
    auto make = [](const CodeObj *c, u32 i, Value *t) {
        Value line = Value::of_int(i32(code_line(c, i)));
        t[0] = t[1] = line;
        t[2] = t[3] = value_none();
    };
    return tuples_iter(u32(code_of(a.args[0])->code.size()), 4, make, a.args[0], out);
}

// co_lines(): (start, end, line), in the two-per-instruction offsets that
// f_lasti and tb_lasti count in.
R m_co_lines(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || !is_code(a.args[0]))
        return err_set("TypeError", "co_lines() takes no arguments");
    const CodeObj *c = code_of(a.args[0]);
    // One range per run of instructions on the same line.
    u32 runs = 0;
    for (u32 i = 0; i < c->code.size(); i++)
        if (i == 0 || code_line(c, i) != code_line(c, i - 1))
            runs++;
    auto make = [](const CodeObj *c, u32 k, Value *t) {
        u32 run = 0, start = 0;
        for (u32 i = 0; i < c->code.size(); i++)
            if (i == 0 || code_line(c, i) != code_line(c, i - 1)) {
                if (run++ == k) {
                    start = i;
                    break;
                }
            }
        u32 end = start + 1;
        while (end < c->code.size() && code_line(c, end) == code_line(c, start))
            end++;
        t[0] = Value::of_int(i32(start * 2));
        t[1] = Value::of_int(i32(end * 2));
        t[2] = Value::of_int(i32(code_line(c, start)));
    };
    return tuples_iter(runs, 3, make, a.args[0], out);
}

// Two code objects are equal when they would run the same: CPython compares
// the names, the flags, the counts, the bytes and the constants.
R code_eq(Value a, Value b, bool &out)
{
    out = false;
    if (!is_code(b))
        return R::NotImpl;
    CodeObj *x = code_of(a), *y = code_of(b);
    if (x == y)
        return out = true, R::Ok;
    if (x->flags != y->flags || x->argcount != y->argcount || x->posonly != y->posonly ||
        x->kwonly != y->kwonly || x->code.size() != y->code.size() ||
        x->consts.size() != y->consts.size() || x->names.size() != y->names.size() ||
        x->varnames.size() != y->varnames.size() || x->cellvars.size() != y->cellvars.size() ||
        x->freevars.size() != y->freevars.size())
        return R::Ok;
    bool same = false;
    if (py_eq(x->name, y->name, same) != R::Ok)
        return R::Err;
    if (!same)
        return R::Ok;
    for (usize i = 0; i < x->code.size(); i++)
        if (x->code[i].op != y->code[i].op || x->code[i].arg != y->code[i].arg)
            return R::Ok;
    const Vec<Value> *lists[5][2] = { { &x->consts, &y->consts },
                                      { &x->names, &y->names },
                                      { &x->varnames, &y->varnames },
                                      { &x->cellvars, &y->cellvars },
                                      { &x->freevars, &y->freevars } };
    for (auto &pair : lists)
        for (usize i = 0; i < pair[0]->size(); i++) {
            Value u = (*pair[0])[i], v = (*pair[1])[i];
            // A const keeps its type: 1 and 1.0 are not the same constant.
            if (u.is_obj() != v.is_obj() || (u.is_obj() && u.obj()->type != v.obj()->type))
                return R::Ok;
            if (py_eq(u, v, same) != R::Ok)
                return R::Err;
            if (!same)
                return R::Ok;
        }
    out = true;
    return R::Ok;
}

R code_hash(Value v, u32 &out)
{
    CodeObj *c = code_of(v);
    u32 h      = c->flags * 31 + c->argcount * 7 + u32(c->code.size());
    for (const Instr &in : c->code)
        h = h * 1000003 + u32(in.op) * 31 + in.arg;
    u32 k = 0;
    if (py_hash(c->name, k) != R::Ok)
        return R::Err;
    out = h ^ k;
    return R::Ok;
}

constexpr Method CODE_METHODS[] = {
    { "replace", m_replace },
    { "co_positions", m_co_positions },
    { "co_lines", m_co_lines },
};

} // namespace

bool code_methods()
{
    return method_install(&code_type, CODE_METHODS);
}

constexpr Type code_type{ .name    = "code",
                          .trace   = code_trace,
                          .fini    = code_fini,
                          .hash    = code_hash,
                          .eq      = code_eq,
                          .repr    = code_repr,
                          .getattr = code_getattr };

Str bc_name(Bc op)
{
    usize i = usize(op);
    return i < NOPS ? NAMES[i] : Str("?");
}

Arg bc_arg(Bc op)
{
    usize i = usize(op);
    return i < NOPS ? ARGS[i] : Arg::None;
}

CodeObj *code_new(Value name, Value filename, u32 firstline)
{
    Root rn{ name }, rf{ filename };
    CodeObj *c = static_cast<CodeObj *>(obj_alloc(&code_type, sizeof(CodeObj)));
    if (!c)
        return nullptr;
    new (&c->code) Vec<Instr>();
    new (&c->consts) Vec<Value>();
    new (&c->names) Vec<Value>();
    new (&c->varnames) Vec<Value>();
    new (&c->cellvars) Vec<Value>();
    new (&c->freevars) Vec<Value>();
    new (&c->lines) Vec<LineEntry>();
    c->name      = rn.v;
    c->qualname  = rn.v;
    c->doc       = Value();
    c->filename  = rf.v;
    c->flags     = 0;
    c->argcount  = 0;
    c->posonly   = 0;
    c->kwonly    = 0;
    c->stacksize = 0;
    c->nblocks   = 0;
    c->firstline = firstline;
    return c;
}

u32 code_line(const CodeObj *c, u32 pc)
{
    u32 line = c->firstline;
    for (usize i = 0; i < c->lines.size(); i++) {
        if (c->lines[i].at > pc)
            break;
        line = c->lines[i].line;
    }
    return line;
}

usize code_nlocals(const CodeObj *c)
{
    return c->varnames.size() + c->cellvars.size() + c->freevars.size();
}
