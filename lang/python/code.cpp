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

} // namespace

constexpr Type code_type{ .name  = "code",
                          .trace = code_trace,
                          .fini  = code_fini,
                          .repr  = code_repr };

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
