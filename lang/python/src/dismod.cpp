// `dis`, this port's own rather than CPython's.
//
// CPython's Lib/dis.py decodes CPython's instruction stream. This one's is an
// opcode and a whole u32 (code.h), so a copy would read the wrong bytes. The
// names here are the ones its callers need: inspect wants
// COMPILER_FLAG_NAMES, get_instructions() and Positions.
#include "builtin.h"
#include "call.h"
#include "code.h"
#include "err.h"
#include "func.h"
#include "gc.h"
#include "info.h"
#include "intern.h"
#include "io.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

INFO_TYPE(positions_type, "dis.Positions");
INFO_TYPE(instruction_type, "dis.Instruction");

constexpr Str POSITION_NAMES[4] = { "lineno", "end_lineno", "col_offset", "end_col_offset" };

constexpr Str INSTRUCTION_NAMES[8] = { "opname",  "opcode", "arg",         "argval",
                                       "argrepr", "offset", "line_number", "positions" };

// `Positions(lineno, end_lineno, col_offset, end_col_offset)`, each of which
// may be left out; a missing one is None, as CPython's namedtuple defaults it.
R b_positions(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs > 4)
        return err_set("TypeError", "Positions() takes at most 4 arguments");
    Value items[4] = { value_none(), value_none(), value_none(), value_none() };
    for (u32 i = 0; i < a.nargs; i++)
        items[i] = a.args[i];
    Roots pin{ items, 4 };
    out = info_new(&positions_type, items, POSITION_NAMES, 4);
    return out.is_nil() ? R::Err : R::Ok;
}

// The line an instruction is on, from the code object's line table.
u32 line_at(const CodeObj *c, u32 at)
{
    u32 line = c->firstline;
    for (usize k = 0; k < c->lines.size(); k++) {
        if (c->lines[k].at > at)
            break;
        line = c->lines[k].line;
    }
    return line;
}

// What an operand names, as a value and as the text a listing would print.
bool operand_of(const CodeObj *c, const Instr &in, Value &val, String &text)
{
    const Vec<Value> *pool = nullptr;
    switch (bc_arg(in.op)) {
    case Arg::Const:
        pool = &c->consts;
        break;
    case Arg::Name:
        pool = &c->names;
        break;
    case Arg::Local:
        pool = &c->varnames;
        break;
    case Arg::Deref:
        if (in.arg < c->cellvars.size()) {
            val = c->cellvars[in.arg];
        } else if (in.arg - c->cellvars.size() < c->freevars.size()) {
            val = c->freevars[in.arg - c->cellvars.size()];
        }
        break;
    case Arg::None:
        val = value_none();
        return true;
    default:
        val = Value::of_int(i32(in.arg));
        break;
    }
    if (pool)
        val = in.arg < pool->size() ? (*pool)[in.arg] : Value::of_int(i32(in.arg));
    if (val.is_nil())
        val = value_none();
    if (bc_arg(in.op) == Arg::Const)
        return py_repr(val, text) == R::Ok;
    if (is_str(val))
        return text.append(str_of(val)->str());
    Buf<24> b;
    b.put(u64(in.arg));
    return text.append(b.str());
}

Value instruction_new(const CodeObj *c, usize at)
{
    const Instr &in = c->code[at];
    Root name{ str_new(bc_name(in.op)) };
    if (name.v.is_nil())
        return Value();
    Root val;
    String text;
    if (!operand_of(c, in, val.v, text))
        return oom(), Value();
    Root repr{ str_new(text.str()) };
    if (repr.v.is_nil())
        return Value();
    u32 line     = line_at(c, u32(at));
    Value pos[4] = { Value::of_int(i32(line)), Value::of_int(i32(line)), value_none(),
                     value_none() };
    Roots ppin{ pos, 4 };
    Root where{ info_new(&positions_type, pos, POSITION_NAMES, 4) };
    if (where.v.is_nil())
        return Value();

    Value items[8] = { name.v,
                       Value::of_int(i32(in.op)),
                       bc_arg(in.op) == Arg::None ? value_none() : Value::of_int(i32(in.arg)),
                       val.v,
                       repr.v,
                       Value::of_int(i32(at)),
                       Value::of_int(i32(line)),
                       where.v };
    Roots pin{ items, 8 };
    return info_new(&instruction_type, items, INSTRUCTION_NAMES, 8);
}

// The code object behind whatever was handed over.
const CodeObj *code_of_any(Value v)
{
    if (is_code(v))
        return code_of(v);
    if (is_func(v))
        return is_code(func_of(v)->code) ? code_of(func_of(v)->code) : nullptr;
    Value inner;
    StrObj *k = str_intern("__code__");
    if (k && py_attr_opt(v, k, inner, Value(), false) == Got::Ok && is_code(inner))
        return code_of(inner);
    return nullptr;
}

R b_get_instructions(const CallArgs &a, Value &out)
{
    if (!args_only(a, "get_instructions", 1, 1))
        return R::Err;
    Root src{ a.args[0] };
    const CodeObj *c = code_of_any(src.v);
    if (!c)
        return err_set2("TypeError", "don't know how to disassemble", type_name(src.v));
    Root l{ obj_value(list_new()) };
    if (l.v.is_nil())
        return oom();
    for (usize k = 0; k < c->code.size(); k++) {
        Root one{ instruction_new(c, k) };
        if (one.v.is_nil())
            return R::Err;
        if (!list_push(static_cast<ListObj *>(l.v.obj()), one.v))
            return oom();
        c = code_of_any(src.v); // the list may have moved the heap under us
    }
    out = l.v;
    return R::Ok;
}

// s[0] the listing. A native cannot reach sys.stdout, so print() does it.
R dis_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        Root empty{ str_new(Str("")) };
        Root names{ obj_value(tuple_new(1)) }, vals{ obj_value(tuple_new(1)) };
        Root args{ obj_value(tuple_new(1)) };
        StrObj *end = str_intern("end");
        if (empty.v.is_nil() || names.v.is_nil() || vals.v.is_nil() || args.v.is_nil() || !end)
            return oom();
        static_cast<TupleObj *>(names.v.obj())->items()[0] = obj_value(end);
        static_cast<TupleObj *>(vals.v.obj())->items()[0]  = empty.v;
        static_cast<TupleObj *>(args.v.obj())->items()[0]  = k->s[0];
        return cont_call_kw(k, k->s[1], args.v, names.v, vals.v);
    }
    return cont_done(k, in);
}

R b_dis(const CallArgs &a, Value &out)
{
    if (a.nargs != 1 || a.nkw)
        return err_set("TypeError", "dis() takes one argument");
    Root src{ a.args[0] };
    const CodeObj *c = code_of_any(src.v);
    if (!c)
        return err_set2("TypeError", "don't know how to disassemble", type_name(src.v));
    String text;
    if (!code_dis(c, text))
        return R::Err;
    Root line{ str_new(text.str()) };
    Root mod{ builtin_module("builtins") };
    StrObj *pk = str_intern("print");
    Value fn;
    if (line.v.is_nil() || mod.v.is_nil() || !pk)
        return R::Err;
    if (dict_get(module_dict(mod.v), obj_value(pk), fn) != R::Ok)
        return err_set("SystemError", "print() is missing");
    Root rf{ fn };
    Root kv{ cont_new(dis_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = line.v;
    cont_of(kv.v)->s[1] = rf.v;
    out                 = kv.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = {
    { "Positions", b_positions },
    { "get_instructions", b_get_instructions },
    { "dis", b_dis },
    { "disassemble", b_dis },
};

// What inspect reads the CO_* constants out of. The numbers are CPython's.
struct Flag {
    u32 bit;
    Str name;
};

constexpr Flag FLAGS[] = {
    { 0x0001, "OPTIMIZED" },       { 0x0002, "NEWLOCALS" },        { 0x0004, "VARARGS" },
    { 0x0008, "VARKEYWORDS" },     { 0x0010, "NESTED" },           { 0x0020, "GENERATOR" },
    { 0x0040, "NOFREE" },          { 0x0080, "COROUTINE" },        { 0x0100, "ITERABLE_COROUTINE" },
    { 0x0200, "ASYNC_GENERATOR" }, { 0x4000000, "HAS_DOCSTRING" }, { 0x8000000, "METHOD" },
};

bool put_flag_names(DictObj *into)
{
    Root rd{ obj_value(into) };
    Root d{ obj_value(dict_new()) };
    if (d.v.is_nil())
        return oom() == R::Ok;
    for (const Flag &f : FLAGS) {
        Root nm{ str_new(f.name) };
        if (nm.v.is_nil() || dict_set(dict_at(d.v), Value::of_int(i32(f.bit)), nm.v) != R::Ok)
            return false;
    }
    return mod_put(static_cast<DictObj *>(rd.v.obj()), "COMPILER_FLAG_NAMES", d.v);
}

// `opname` and `opmap`, over this port's instruction set.
bool put_opnames(DictObj *into)
{
    Root rd{ obj_value(into) };
    Root names{ obj_value(list_new()) };
    Root map{ obj_value(dict_new()) };
    if (names.v.is_nil() || map.v.is_nil())
        return oom() == R::Ok;
    for (u32 k = 0;; k++) {
        Str n = bc_name(Bc(k));
        if (n == "?")
            break;
        Root nm{ str_new(n) };
        if (nm.v.is_nil() || !list_push(static_cast<ListObj *>(names.v.obj()), nm.v) ||
            dict_set(dict_at(map.v), nm.v, Value::of_int(i32(k))) != R::Ok)
            return false;
    }
    return mod_put(static_cast<DictObj *>(rd.v.obj()), "opname", names.v) &&
           mod_put(static_cast<DictObj *>(rd.v.obj()), "opmap", map.v);
}

} // namespace

bool dismod_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!mod_defs(static_cast<DictObj *>(rd.v.obj()), DEFS))
        return false;
    if (!put_flag_names(static_cast<DictObj *>(rd.v.obj())) ||
        !put_opnames(static_cast<DictObj *>(rd.v.obj())))
        return false;
    return mod_str(static_cast<DictObj *>(rd.v.obj()), "__doc__",
                   "Disassemble this interpreter's own bytecode.");
}
