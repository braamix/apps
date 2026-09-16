// The code object: allocation, tracing, the line table, and the two tables the
// opcode list generates.
#include "code.h"

#include "gc.h"
#include "kernel/fmt.h"

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

} // namespace

constexpr Type code_type{ .name    = "code",
                          .trace   = code_trace,
                          .fini    = code_fini,
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
