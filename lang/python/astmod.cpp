// `_ast`: the node classes CPython's ast module is written against, and the
// tree `compile()` answers for PyCF_ONLY_AST.
//
// A class is a heap type made at install rather than a static descriptor, so
// a program may subclass one and ast.py's visitors need nothing said about
// them; an instance keeps its fields in its own dict, which is what CPython
// does too. The table below is CPython's ASDL, taken from the clone's own
// classes: name, base, the fields with their types, the fields that default
// to None, and whether the kind carries a position.
//
// The tree is built out of the parser's arena, node for node, the way
// astdump.cpp prints it; astpos.cpp says where each one is.
#include "astmod.h"

#include "astpos.h"
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "complex.h"
#include "err.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"
#include "type.h"
#include "union.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

DictObj *dict_at(Value v)
{
    return static_cast<DictObj *>(v.obj());
}

ListObj *list_at(Value v)
{
    return static_cast<ListObj *>(v.obj());
}

Value tuple_of(const Value *items, usize n)
{
    TupleObj *t = tuple_new(n);
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < n; i++)
        t->items()[i] = items[i];
    return obj_value(t);
}

struct Cls {
    Str name;
    Str base;
    Str fields; // `name:type` pairs, two spaces between; `*` a list, `?` optional
    Str nones;  // the fields whose default is None, comma separated
    bool pos;   // carries lineno, col_offset, end_lineno, end_col_offset
};

constexpr Cls CLASSES[] = {
#include "asttab.h"
};

constexpr usize NCLASSES = sizeof(CLASSES) / sizeof(CLASSES[0]);

// Every class, by the index of its row, plus the AST base at the end.
struct Home {
    Value cls[NCLASSES + 1];
    Value ctx[3];   // Load, Store and Del, made once
    Value ops[32];  // the operator, unaryop, cmpop and boolop singletons
    Value module;   // the _ast module's namespace
};

Home *home = nullptr;

void home_mark()
{
    if (!home)
        return;
    for (usize i = 0; i <= NCLASSES; i++)
        gc_mark(home->cls[i]);
    for (usize i = 0; i < 3; i++)
        gc_mark(home->ctx[i]);
    for (usize i = 0; i < 32; i++)
        gc_mark(home->ops[i]);
    gc_mark(home->module);
}

// ------------------------------------------------------------- the classes

// The next `sep`-separated piece of `s`, which is advanced past it.
Str next_word(Str &s, char sep)
{
    usize i = 0;
    while (i < s.size() && s[i] != sep)
        i++;
    Str word = s.substr(0, i);
    while (i < s.size() && s[i] == sep)
        i++;
    s = s.substr(i, s.size() - i);
    return word;
}

bool holds_word(Str list, Str want)
{
    Str rest = list;
    while (rest.size()) {
        if (next_word(rest, ',') == want)
            return true;
    }
    return false;
}

usize class_index(Str name)
{
    for (usize i = 0; i < NCLASSES; i++)
        if (CLASSES[i].name == name)
            return i;
    return NCLASSES; // AST
}

Value class_by_name(Str name)
{
    return home->cls[class_index(name)];
}

// `stmt`, `expr*`, `expr?` as ast.py wants to read it back: the class, a
// `list[T]`, or `T | None`.
Value field_type(Str spec)
{
    bool list = spec.size() && spec[spec.size() - 1] == '*';
    bool opt  = spec.size() && spec[spec.size() - 1] == '?';
    Str base  = list || opt ? spec.substr(0, spec.size() - 1) : spec;

    Root inner;
    if (base == "str")
        inner = type_wrap(&str_type);
    else if (base == "int")
        inner = type_wrap(&int_type);
    else if (base == "constant" || base == "object")
        inner = type_wrap(&object_type);
    else
        inner = class_by_name(base);
    if (inner.v.is_nil())
        inner = type_wrap(&object_type);
    if (inner.v.is_nil())
        return Value();

    if (list) {
        Root lt{ type_wrap(&list_type) };
        return lt.v.is_nil() ? Value() : genalias_new(lt.v, inner.v);
    }
    if (opt) {
        Value two[2] = { inner.v, value_none() };
        Root pair{ tuple_of(two, 2) };
        return pair.v.is_nil() ? Value() : union_from(pair.v);
    }
    return inner.v;
}

// AST.__init__: the fields in order, then whatever keywords name.
R n_ast_init(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "__init__() needs an instance");
    Root self{ a.args[0] };
    Root fields;
    StrObj *fk = str_intern("_fields");
    if (!fk)
        return oom();
    Value cls = type_of_value(self.v);
    if (cls.is_nil())
        return R::Err;
    Root rc{ cls };
    if (type_lookup(rc.v, fk, fields.v) != R::Ok || !is_tuple(fields.v))
        return err_set("TypeError", "this class has no _fields");

    TupleObj *f = static_cast<TupleObj *>(fields.v.obj());
    if (a.nargs - 1 > f->len) {
        Buf<96> b;
        b.put(type_name(self.v)).put(" takes at most ").put(u64(f->len)).put(" arguments");
        return err_set("TypeError", b.str());
    }
    for (u32 i = 1; i < a.nargs; i++) {
        Value fn = static_cast<TupleObj *>(fields.v.obj())->items()[i - 1];
        if (inst_setattr(self.v, str_of(fn), a.args[i]) != R::Ok)
            return R::Err;
    }
    for (u32 i = 0; i < a.nkw; i++)
        if (inst_setattr(self.v, str_of(a.kwnames[i]), a.kwvals[i]) != R::Ok)
            return R::Err;
    out = value_none();
    return R::Ok;
}

// `Call(func=Name(...), args=[...])`, as CPython's own repr writes it: three
// levels deep, and a list shown by its first and last item alone.
bool repr_node(Value v, String &out, i32 depth);

bool repr_list(Value v, String &out, i32 depth)
{
    Root rv{ v };
    bool list  = is_list(rv.v);
    usize len  = list ? list_at(rv.v)->items.size() : static_cast<TupleObj *>(rv.v.obj())->len;
    if (!len)
        return py_repr(rv.v, out) == R::Ok;
    if (!out.push(list ? '[' : '('))
        return false;
    for (usize i = 0; i < (len < 2 ? len : 2); i++) {
        usize at = i ? len - 1 : 0;
        if (i && !out.append(", "))
            return false;
        Value item = list ? list_at(rv.v)->items[at]
                          : static_cast<TupleObj *>(rv.v.obj())->items()[at];
        if (!(is_inst(item) ? repr_node(item, out, depth - 1) : py_repr(item, out) == R::Ok))
            return false;
        if (!i && len > 2 && !out.append(", ..."))
            return false;
    }
    return out.push(list ? ']' : ')');
}

bool repr_node(Value v, String &out, i32 depth)
{
    Root rv{ v };
    Str name = type_name(rv.v);
    Root cls{ type_of_value(rv.v) };
    Root fields;
    StrObj *fk = str_intern("_fields");
    if (cls.v.is_nil() || !fk)
        return false;
    if (type_lookup(cls.v, fk, fields.v) != R::Ok || !is_tuple(fields.v))
        return false;
    usize n = static_cast<TupleObj *>(fields.v.obj())->len;
    if (!out.append(name))
        return false;
    if (!n)
        return out.append("()");
    if (depth <= 0)
        return out.append("(...)");
    if (!out.push('('))
        return false;
    for (usize i = 0; i < n; i++) {
        Root fn{ static_cast<TupleObj *>(fields.v.obj())->items()[i] };
        Root got;
        if (py_attr(rv.v, str_of(fn.v), got.v) != Got::Ok)
            return false;
        if (i && !out.append(", "))
            return false;
        if (!out.append(str_of(fn.v)->str()) || !out.push('='))
            return false;
        bool seq = is_list(got.v) || is_tuple(got.v);
        if (!(seq        ? repr_list(got.v, out, depth)
              : is_inst(got.v) ? repr_node(got.v, out, depth - 1)
                               : py_repr(got.v, out) == R::Ok))
            return false;
    }
    return out.push(')');
}

R n_ast_repr(const CallArgs &a, Value &out)
{
    if (!a.nargs)
        return err_set("TypeError", "__repr__() needs an instance");
    String b;
    if (!repr_node(a.args[0], b, 3))
        return err_pending() ? R::Err : oom();
    out = str_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

// One class of the table, with `_fields`, `__match_args__`, `_field_types`
// and, for the kinds that carry one, `_attributes`.
bool make_class(usize i, Value base)
{
    const Cls &c = CLASSES[i];
    Root rb{ base };
    Root d{ obj_value(dict_new()) };
    if (d.v.is_nil())
        return oom() == R::Ok;

    // The field names, and the type of each beside it.
    usize count = 0;
    for (Str rest = c.fields; rest.size();) {
        next_word(rest, ' ');
        count++;
    }
    Root names{ obj_value(tuple_new(count)) };
    if (names.v.is_nil())
        return oom() == R::Ok;
    usize k = 0;
    for (Str rest = c.fields; rest.size(); k++) {
        Str one   = next_word(rest, ' ');
        Str fname = next_word(one, ':');
        StrObj *n = str_intern(fname);
        if (!n)
            return oom() == R::Ok;
        static_cast<TupleObj *>(names.v.obj())->items()[k] = obj_value(n);
        // A field that may be left out answers None rather than nothing.
        if (holds_word(c.nones, fname) &&
            dict_set(dict_at(d.v), obj_value(n), value_none()) != R::Ok)
            return false;
    }

    Root attrs;
    if (c.pos) {
        constexpr Str POS[4] = { "lineno", "col_offset", "end_lineno", "end_col_offset" };
        attrs                = obj_value(tuple_new(4));
        if (attrs.v.is_nil())
            return oom() == R::Ok;
        for (usize p = 0; p < 4; p++) {
            StrObj *n = str_intern(POS[p]);
            if (!n)
                return oom() == R::Ok;
            static_cast<TupleObj *>(attrs.v.obj())->items()[p] = obj_value(n);
        }
    }

    struct Put {
        Str key;
        Value v;
    };
    Put puts[3] = { { "_fields", names.v },
                    { "__match_args__", names.v },
                    { "_attributes", attrs.v } };
    for (Put &p : puts) {
        if (p.v.is_nil())
            continue;
        StrObj *n = str_intern(p.key);
        if (!n || dict_set(dict_at(d.v), obj_value(n), p.v) != R::Ok)
            return oom() == R::Ok;
    }

    Root nm{ obj_value(str_intern(c.name)) };
    Root bases{ obj_value(tuple_new(1)) };
    if (nm.v.is_nil() || bases.v.is_nil())
        return oom() == R::Ok;
    static_cast<TupleObj *>(bases.v.obj())->items()[0] = rb.v;
    Value made = type_new(nm.v, bases.v, d.v);
    if (made.is_nil())
        return false;
    home->cls[i] = made;
    return true;
}

// `_field_types`, once every class exists: a field may name one further down
// the table, and ast.dump reads the answer back to tell a context from a
// list it may leave out.
bool fill_types(usize i)
{
    const Cls &c = CLASSES[i];
    Root cls{ home->cls[i] };
    Root types{ obj_value(dict_new()) };
    StrObj *key = str_intern("_field_types");
    if (types.v.is_nil() || !key)
        return oom() == R::Ok;
    for (Str rest = c.fields; rest.size();) {
        Str one   = next_word(rest, ' ');
        StrObj *n = str_intern(next_word(one, ':'));
        if (!n)
            return oom() == R::Ok;
        Root ft{ field_type(one) };
        if (ft.v.is_nil() || dict_set(dict_at(types.v), obj_value(n), ft.v) != R::Ok)
            return false;
    }
    Value fn;
    return attr_store(cls.v, key, types.v, fn) == R::Ok;
}

} // namespace

// --------------------------------------------------------------- the tree

namespace {

struct Builder {
    const Ast *ast;
    bool ok = true;

    u32 kid(const Node &n, usize k) const { return ast->kids[n.kid0 + k]; }

    Value fail()
    {
        ok = false;
        return Value();
    }

    // A fresh instance of `name`, with its position filled in from `at`.
    Value make(Str name, u32 at);

    bool set(Value obj, Str field, Value v);
    bool set_list(Value obj, Str field, const Node &n, usize from, usize count);

    Value node(u32 i);
    Value constant_of(const Node &n);
    Value list_of(const Node &n, usize from, usize count);

    // The parser does not record what an expression is read or written for,
    // so a target gets its context once the tree under it is built.
    bool retarget(Value v, usize ctx);
    bool retarget_list(Value v, usize ctx);
};

Value Builder::make(Str name, u32 at)
{
    Value cls = class_by_name(name);
    if (cls.is_nil())
        return fail();
    Root rc{ cls };
    Root o{ inst_new(rc.v) };
    if (o.v.is_nil())
        return fail();
    if (!at)
        return o.v;
    StrObj *has = str_intern("_attributes");
    Value found;
    if (!has || type_lookup(rc.v, has, found) != R::Ok)
        return o.v;
    Pos p                = ast_pos(*ast, at);
    constexpr Str N[4]   = { "lineno", "col_offset", "end_lineno", "end_col_offset" };
    const u32 value[4]   = { p.line, p.col, p.eline, p.ecol };
    for (usize k = 0; k < 4; k++)
        if (!set(o.v, N[k], Value::of_int(i32(value[k]))))
            return Value();
    return o.v;
}

bool Builder::set(Value obj, Str field, Value v)
{
    if (!ok || v.is_nil())
        return ok = false;
    Root ro{ obj }, rv{ v };
    StrObj *n = str_intern(field);
    if (!n)
        return ok = false;
    return (ok = inst_setattr(ro.v, n, rv.v) == R::Ok);
}

Value Builder::list_of(const Node &n, usize from, usize count)
{
    Root l{ obj_value(list_new()) };
    if (l.v.is_nil())
        return fail();
    for (usize k = 0; k < count; k++) {
        Root item{ node(kid(n, from + k)) };
        if (!ok)
            return Value();
        if (!list_push(list_at(l.v), item.v))
            return fail();
    }
    return l.v;
}

bool Builder::set_list(Value obj, Str field, const Node &n, usize from, usize count)
{
    Root ro{ obj };
    Root l{ list_of(n, from, count) };
    return ok && set(ro.v, field, l.v);
}

} // namespace

namespace {

// The operator singletons, by the enum the parser stored.
constexpr Str BINOPS[] = { "Add",    "Sub",    "Mult",   "Div",  "FloorDiv", "Mod",    "Pow",
                           "BitAnd", "BitOr",  "BitXor", "LShift", "RShift", "MatMult" };
constexpr Str CMPOPS[] = { "Eq", "NotEq", "Lt", "LtE", "Gt", "GtE", "In", "NotIn", "Is", "IsNot" };
constexpr Str UNOPS[]  = { "Invert", "Not", "UAdd", "USub" };

// A singleton kept in `ops`, made on first use. The index is the class's own
// row, so two asks answer the same object, as CPython's do.
Value op_of(Str name)
{
    usize i = class_index(name);
    for (usize k = 0; k < 32; k++) {
        if (home->ops[k].is_nil()) {
            Root c{ home->cls[i] };
            if (c.v.is_nil())
                return Value();
            Value made = inst_new(c.v);
            if (made.is_nil())
                return Value();
            home->ops[k] = made;
            return made;
        }
        if (type_of_value(home->ops[k]).obj() == home->cls[i].obj())
            return home->ops[k];
    }
    Root c{ home->cls[i] };
    return c.v.is_nil() ? Value() : inst_new(c.v);
}

// Load(), the context every expression this parser makes is read in. A target
// is rewritten by ast.py's users rather than here, which is what CPython's
// own `ast` does for a tree it did not compile.
Value ctx_of(usize k)
{
    if (home->ctx[k].is_nil()) {
        constexpr Str NAME[3] = { "Load", "Store", "Del" };
        Root c{ class_by_name(NAME[k]) };
        if (c.v.is_nil())
            return Value();
        home->ctx[k] = inst_new(c.v);
    }
    return home->ctx[k];
}

Value ctx_load()
{
    return ctx_of(0);
}

Value Builder::constant_of(const Node &n)
{
    const Token &t = ast->lex.tokens[n.tok];
    switch (Const(n.flags)) {
    case Const::None:
        return value_none();
    case Const::True:
        return value_bool(true);
    case Const::False:
        return value_bool(false);
    case Const::Ellipsis:
        return value_ellipsis();
    case Const::Int:
        if (t.flags & TOK_INT_WIDE)
            return int_parse(ast->lex.text_of(t), tok_int_base(t.flags));
        return big_from_i64(t.ival);
    case Const::Imag:
        return complex_new(0, t.fval);
    case Const::Float:
        return float_new(t.fval);
    case Const::Bytes:
        return bytes_new(ast->lex.text_of(t));
    case Const::Str:
        break;
    }
    StrObj *s = str_raw(ast->lex.text_of(t));
    return s ? obj_value(s) : Value();
}

} // namespace

namespace {

// The identifier a node carries, as a str.
Value ident_of(const Ast *ast, u32 i)
{
    StrObj *s = str_intern(ast->text(i));
    return s ? obj_value(s) : Value();
}

// A dotted name, kept as the span of tokens it was written as.
Value dotted_of(const Ast *ast, u32 from, u32 to)
{
    String b;
    for (u32 k = from; k < to; k++) {
        const Token &t = ast->lex.tokens[k];
        if (!(t.kind == Tok::Name ? b.append(ast->lex.text_of(t)) : b.push('.')))
            return oom(), Value();
    }
    return str_new(b.str());
}

// Store or Del, down through the shapes an assignment target may take.
bool Builder::retarget(Value v, usize ctx)
{
    if (!ok || !is_inst(v))
        return ok;
    Root rv{ v };
    Str k = type_name(rv.v);
    if (k == "Name" || k == "Attribute" || k == "Subscript") {
        Root c{ ctx_of(ctx) };
        return set(rv.v, "ctx", c.v);
    }
    if (k == "Starred") {
        Root c{ ctx_of(ctx) };
        if (!set(rv.v, "ctx", c.v))
            return false;
        Root in;
        StrObj *n = str_intern("value");
        if (!n || py_attr(rv.v, n, in.v) != Got::Ok)
            return ok = false;
        return retarget(in.v, ctx);
    }
    if (k == "List" || k == "Tuple") {
        Root c{ ctx_of(ctx) };
        if (!set(rv.v, "ctx", c.v))
            return false;
        Root elts;
        StrObj *n = str_intern("elts");
        if (!n || py_attr(rv.v, n, elts.v) != Got::Ok)
            return ok = false;
        return retarget_list(elts.v, ctx);
    }
    return ok;
}

bool Builder::retarget_list(Value v, usize ctx)
{
    if (!ok || !is_list(v))
        return ok;
    Root rv{ v };
    for (usize k = 0; k < list_at(rv.v)->items.size(); k++)
        if (!retarget(list_at(rv.v)->items[k], ctx))
            return false;
    return ok;
}

Value Builder::node(u32 i)
{
    if (!ok)
        return Value();
    if (!i)
        return value_none();
    const Node &n = ast->at(i);

    switch (n.kind) {
    case Nd::Module: {
        Root o{ make("Module", 0) };
        if (!ok || !set_list(o.v, "body", n, 0, n.nkid))
            return Value();
        Root e{ obj_value(list_new()) };
        return e.v.is_nil() ? fail() : (set(o.v, "type_ignores", e.v) ? o.v : Value());
    }

    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef: {
        Root o{ make(n.kind == Nd::FunctionDef ? Str("FunctionDef") : Str("AsyncFunctionDef"), i) };
        Root nm{ ident_of(ast, i) };
        Root args{ node(n.a) };
        if (!ok || !set(o.v, "name", nm.v) || !set(o.v, "args", args.v) ||
            !set_list(o.v, "body", n, 0, n.b) ||
            !set_list(o.v, "decorator_list", n, n.b, n.c))
            return Value();
        Root ret{ node(n.d) };
        if (!ok || !set(o.v, "returns", ret.v) ||
            !set_list(o.v, "type_params", n, n.b + n.c, n.pad))
            return Value();
        return o.v;
    }

    case Nd::ClassDef: {
        Root o{ make("ClassDef", i) };
        Root nm{ ident_of(ast, i) };
        if (!ok || !set(o.v, "name", nm.v) || !set_list(o.v, "bases", n, 0, n.a) ||
            !set_list(o.v, "keywords", n, n.a, n.b) ||
            !set_list(o.v, "body", n, n.a + n.b, n.c) ||
            !set_list(o.v, "decorator_list", n, n.a + n.b + n.c, n.d) ||
            !set_list(o.v, "type_params", n, n.a + n.b + n.c + n.d, n.pad))
            return Value();
        return o.v;
    }

    case Nd::Return:
    case Nd::Delete:
    case Nd::Expr:
    case Nd::Await:
    case Nd::Yield:
    case Nd::YieldFrom:
    case Nd::Starred: {
        constexpr Str NAME[] = { "Return", "Delete", "Expr", "Await", "Yield", "YieldFrom",
                                 "Starred" };
        usize k = n.kind == Nd::Return      ? 0
                  : n.kind == Nd::Delete    ? 1
                  : n.kind == Nd::Expr      ? 2
                  : n.kind == Nd::Await     ? 3
                  : n.kind == Nd::Yield     ? 4
                  : n.kind == Nd::YieldFrom ? 5
                                            : 6;
        Root o{ make(NAME[k], i) };
        if (!ok)
            return Value();
        if (n.kind == Nd::Delete) {
            Root l{ list_of(n, 0, n.nkid) };
            if (!ok || !retarget_list(l.v, 2) || !set(o.v, "targets", l.v))
                return Value();
            return o.v;
        }
        Root v{ node(n.a) };
        if (!ok || !set(o.v, "value", v.v))
            return Value();
        if (n.kind == Nd::Starred) {
            Root c{ ctx_load() };
            if (!set(o.v, "ctx", c.v))
                return Value();
        }
        return o.v;
    }

    case Nd::Global:
    case Nd::Nonlocal: {
        Root o{ make(n.kind == Nd::Global ? Str("Global") : Str("Nonlocal"), i) };
        Root l{ obj_value(list_new()) };
        if (!ok || l.v.is_nil())
            return fail();
        for (u32 k = 0; k < n.nkid; k++) {
            Root nm{ ident_of(ast, kid(n, k)) };
            if (nm.v.is_nil() || !list_push(list_at(l.v), nm.v))
                return fail();
        }
        return set(o.v, "names", l.v) ? o.v : Value();
    }

    case Nd::Import: {
        Root o{ make("Import", i) };
        if (!ok || !set_list(o.v, "names", n, 0, n.nkid) ||
            !set(o.v, "is_lazy", Value::of_int((n.pad & 1) ? 1 : 0)))
            return Value();
        return o.v;
    }

    case Nd::ImportFrom: {
        Root o{ make("ImportFrom", i) };
        Root mod{ n.flags ? dotted_of(ast, n.tok, n.b) : value_none() };
        if (!ok || mod.v.is_nil() || !set(o.v, "module", mod.v) ||
            !set_list(o.v, "names", n, 0, n.nkid) ||
            !set(o.v, "level", Value::of_int(i32(n.a))) ||
            !set(o.v, "is_lazy", Value::of_int((n.pad & 1) ? 1 : 0)))
            return Value();
        return o.v;
    }

    case Nd::Assign: {
        Root o{ make("Assign", i) };
        Root v{ node(n.a) };
        Root l{ list_of(n, 0, n.nkid) };
        if (!ok || !retarget_list(l.v, 1) || !set(o.v, "targets", l.v) ||
            !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::AugAssign: {
        Root o{ make("AugAssign", i) };
        Root t{ node(n.a) }, op{ op_of(BINOPS[n.flags]) }, v{ node(n.b) };
        if (!ok || !retarget(t.v, 1) || !set(o.v, "target", t.v) || !set(o.v, "op", op.v) ||
            !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::AnnAssign: {
        Root o{ make("AnnAssign", i) };
        Root t{ node(n.a) }, an{ node(n.b) }, v{ node(n.c) };
        if (!ok || !retarget(t.v, 1) || !set(o.v, "target", t.v) || !set(o.v, "annotation", an.v) ||
            !set(o.v, "value", v.v) ||
            !set(o.v, "simple", Value::of_int((n.flags & 1) ? 1 : 0)))
            return Value();
        return o.v;
    }

    case Nd::For:
    case Nd::AsyncFor: {
        Root o{ make(n.kind == Nd::For ? Str("For") : Str("AsyncFor"), i) };
        Root t{ node(n.a) }, it{ node(n.b) };
        if (!ok || !retarget(t.v, 1) || !set(o.v, "target", t.v) || !set(o.v, "iter", it.v) ||
            !set_list(o.v, "body", n, 0, n.c) || !set_list(o.v, "orelse", n, n.c, n.d))
            return Value();
        return o.v;
    }

    case Nd::While:
    case Nd::If: {
        Root o{ make(n.kind == Nd::While ? Str("While") : Str("If"), i) };
        Root t{ node(n.a) };
        if (!ok || !set(o.v, "test", t.v) || !set_list(o.v, "body", n, 0, n.b) ||
            !set_list(o.v, "orelse", n, n.b, n.c))
            return Value();
        return o.v;
    }

    case Nd::With:
    case Nd::AsyncWith: {
        Root o{ make(n.kind == Nd::With ? Str("With") : Str("AsyncWith"), i) };
        if (!ok || !set_list(o.v, "items", n, 0, n.a) || !set_list(o.v, "body", n, n.a, n.b))
            return Value();
        return o.v;
    }

    case Nd::Raise: {
        Root o{ make("Raise", i) };
        Root e{ node(n.a) }, c{ node(n.b) };
        if (!ok || !set(o.v, "exc", e.v) || !set(o.v, "cause", c.v))
            return Value();
        return o.v;
    }

    case Nd::Try:
    case Nd::TryStar: {
        Root o{ make(n.kind == Nd::Try ? Str("Try") : Str("TryStar"), i) };
        if (!ok || !set_list(o.v, "body", n, 0, n.a) ||
            !set_list(o.v, "handlers", n, n.a, n.b) ||
            !set_list(o.v, "orelse", n, n.a + n.b, n.c) ||
            !set_list(o.v, "finalbody", n, n.a + n.b + n.c, n.d))
            return Value();
        return o.v;
    }

    case Nd::Assert: {
        Root o{ make("Assert", i) };
        Root t{ node(n.a) }, m{ node(n.b) };
        if (!ok || !set(o.v, "test", t.v) || !set(o.v, "msg", m.v))
            return Value();
        return o.v;
    }

    case Nd::Pass:
        return make("Pass", i);
    case Nd::Break:
        return make("Break", i);
    case Nd::Continue:
        return make("Continue", i);

    case Nd::BoolOp: {
        Root o{ make("BoolOp", i) };
        Root op{ op_of(Bool(n.flags) == Bool::And ? Str("And") : Str("Or")) };
        if (!ok || !set(o.v, "op", op.v) || !set_list(o.v, "values", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::NamedExpr: {
        Root o{ make("NamedExpr", i) };
        Root t{ node(n.a) }, v{ node(n.b) };
        if (!ok || !retarget(t.v, 1) || !set(o.v, "target", t.v) || !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::BinOp: {
        Root o{ make("BinOp", i) };
        Root l{ node(n.a) }, op{ op_of(BINOPS[n.flags]) }, r{ node(n.b) };
        if (!ok || !set(o.v, "left", l.v) || !set(o.v, "op", op.v) || !set(o.v, "right", r.v))
            return Value();
        return o.v;
    }

    case Nd::UnaryOp: {
        Root o{ make("UnaryOp", i) };
        Root op{ op_of(UNOPS[n.flags & 3]) }, v{ node(n.a) };
        if (!ok || !set(o.v, "op", op.v) || !set(o.v, "operand", v.v))
            return Value();
        return o.v;
    }

    case Nd::Lambda: {
        Root o{ make("Lambda", i) };
        Root a{ node(n.a) }, b{ node(n.b) };
        if (!ok || !set(o.v, "args", a.v) || !set(o.v, "body", b.v))
            return Value();
        return o.v;
    }

    case Nd::IfExp: {
        Root o{ make("IfExp", i) };
        Root t{ node(n.a) }, b{ node(n.b) }, e{ node(n.c) };
        if (!ok || !set(o.v, "test", t.v) || !set(o.v, "body", b.v) || !set(o.v, "orelse", e.v))
            return Value();
        return o.v;
    }

    case Nd::Dict: {
        Root o{ make("Dict", i) };
        Root keys{ obj_value(list_new()) }, vals{ obj_value(list_new()) };
        if (!ok || keys.v.is_nil() || vals.v.is_nil())
            return fail();
        for (u32 k = 0; k * 2 < n.nkid; k++) {
            u32 kn = kid(n, k * 2);
            Root kv{ kn ? node(kn) : value_none() };
            Root vv{ node(kid(n, k * 2 + 1)) };
            if (!ok || !list_push(list_at(keys.v), kv.v) || !list_push(list_at(vals.v), vv.v))
                return fail();
        }
        if (!set(o.v, "keys", keys.v) || !set(o.v, "values", vals.v))
            return Value();
        return o.v;
    }

    case Nd::Set: {
        Root o{ make("Set", i) };
        if (!ok || !set_list(o.v, "elts", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::List:
    case Nd::Tuple: {
        Root o{ make(n.kind == Nd::List ? Str("List") : Str("Tuple"), i) };
        Root c{ ctx_load() };
        if (!ok || !set_list(o.v, "elts", n, 0, n.nkid) || !set(o.v, "ctx", c.v))
            return Value();
        return o.v;
    }

    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::GeneratorExp: {
        Root o{ make(n.kind == Nd::ListComp     ? "ListComp"
                     : n.kind == Nd::SetComp    ? Str("SetComp")
                                                : Str("GeneratorExp"),
                     i) };
        Root e{ node(n.a) };
        if (!ok || !set(o.v, "elt", e.v) || !set_list(o.v, "generators", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::DictComp: {
        Root o{ make("DictComp", i) };
        Root k{ node(n.a) }, v{ node(n.b) };
        if (!ok || !set(o.v, "key", k.v) || !set(o.v, "value", v.v) ||
            !set_list(o.v, "generators", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::Compare: {
        Root o{ make("Compare", i) };
        Root l{ node(n.a) };
        Root ops{ obj_value(list_new()) }, rhs{ obj_value(list_new()) };
        if (!ok || ops.v.is_nil() || rhs.v.is_nil() || !set(o.v, "left", l.v))
            return Value();
        for (u32 k = 0; k < n.nkid; k++) {
            const Node &c = ast->at(kid(n, k));
            Root op{ op_of(CMPOPS[c.flags]) };
            Root r{ node(c.a) };
            if (!ok || op.v.is_nil() || !list_push(list_at(ops.v), op.v) ||
                !list_push(list_at(rhs.v), r.v))
                return fail();
        }
        if (!set(o.v, "ops", ops.v) || !set(o.v, "comparators", rhs.v))
            return Value();
        return o.v;
    }

    case Nd::Call: {
        Root o{ make("Call", i) };
        Root f{ node(n.a) };
        if (!ok || !set(o.v, "func", f.v) || !set_list(o.v, "args", n, 0, n.b) ||
            !set_list(o.v, "keywords", n, n.b, n.nkid - n.b))
            return Value();
        return o.v;
    }

    case Nd::JoinedStr:
    case Nd::TemplateStr: {
        Root o{ make(n.kind == Nd::JoinedStr ? Str("JoinedStr") : Str("TemplateStr"), i) };
        if (!ok || !set_list(o.v, "values", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::FormattedValue:
    case Nd::Interpolation: {
        bool fv = n.kind == Nd::FormattedValue;
        Root o{ make(fv ? Str("FormattedValue") : Str("Interpolation"), i) };
        Root v{ node(n.a) }, spec{ node(n.b) };
        if (!ok || !set(o.v, "value", v.v) || !set(o.v, "format_spec", spec.v) ||
            !set(o.v, "conversion", Value::of_int(n.flags ? i32(n.flags) : -1)))
            return Value();
        if (!fv) {
            Root t{ node(n.c) };
            if (!ok || !set(o.v, "str", t.v))
                return Value();
        }
        return o.v;
    }

    case Nd::Constant: {
        Root o{ make("Constant", i) };
        Root v{ constant_of(n) };
        if (!ok || v.v.is_nil() || !set(o.v, "value", v.v) || !set(o.v, "kind", value_none()))
            return Value();
        return o.v;
    }

    case Nd::Attribute: {
        Root o{ make("Attribute", i) };
        Root v{ node(n.a) }, nm{ ident_of(ast, i) }, c{ ctx_load() };
        if (!ok || !set(o.v, "value", v.v) || !set(o.v, "attr", nm.v) || !set(o.v, "ctx", c.v))
            return Value();
        return o.v;
    }

    case Nd::Subscript: {
        Root o{ make("Subscript", i) };
        Root v{ node(n.a) }, s{ node(n.b) }, c{ ctx_load() };
        if (!ok || !set(o.v, "value", v.v) || !set(o.v, "slice", s.v) || !set(o.v, "ctx", c.v))
            return Value();
        return o.v;
    }

    case Nd::Name: {
        Root o{ make("Name", i) };
        Root nm{ ident_of(ast, i) }, c{ ctx_load() };
        if (!ok || !set(o.v, "id", nm.v) || !set(o.v, "ctx", c.v))
            return Value();
        return o.v;
    }

    case Nd::Slice: {
        Root o{ make("Slice", i) };
        Root a{ node(n.a) }, b{ node(n.b) }, c{ node(n.c) };
        if (!ok || !set(o.v, "lower", a.v) || !set(o.v, "upper", b.v) || !set(o.v, "step", c.v))
            return Value();
        return o.v;
    }

    case Nd::Comprehen: {
        Root o{ make("comprehension", 0) };
        Root t{ node(n.a) }, it{ node(n.b) };
        if (!ok || !retarget(t.v, 1) || !set(o.v, "target", t.v) || !set(o.v, "iter", it.v) ||
            !set_list(o.v, "ifs", n, 0, n.nkid) ||
            !set(o.v, "is_async", Value::of_int((n.flags & 1) ? 1 : 0)))
            return Value();
        return o.v;
    }

    case Nd::ExceptHandler: {
        Root o{ make("ExceptHandler", i) };
        Root t{ node(n.a) };
        Root nm{ (n.flags & 1) ? ident_of(ast, i) : value_none() };
        if (!ok || nm.v.is_nil() || !set(o.v, "type", t.v) || !set(o.v, "name", nm.v) ||
            !set_list(o.v, "body", n, 0, n.b))
            return Value();
        return o.v;
    }

    case Nd::Arguments: {
        Root o{ make("arguments", 0) };
        u32 vararg = (n.flags & ARG_VARARG) ? 1 : 0;
        u32 kwarg  = (n.flags & ARG_KWARG) ? 1 : 0;
        u32 at     = 0;
        if (!ok || !set_list(o.v, "posonlyargs", n, at, n.a))
            return Value();
        at += n.a;
        if (!set_list(o.v, "args", n, at, n.b))
            return Value();
        at += n.b;
        Root va{ vararg ? node(kid(n, at)) : value_none() };
        if (!ok || !set(o.v, "vararg", va.v))
            return Value();
        at += vararg;
        if (!set_list(o.v, "kwonlyargs", n, at, n.c))
            return Value();
        at += n.c;
        Root kd{ obj_value(list_new()) };
        if (kd.v.is_nil())
            return fail();
        for (u32 k = 0; k < n.c; k++) {
            u32 d = kid(n, at + k);
            Root v{ d ? node(d) : value_none() };
            if (!ok || !list_push(list_at(kd.v), v.v))
                return fail();
        }
        if (!set(o.v, "kw_defaults", kd.v))
            return Value();
        at += n.c;
        Root kw{ kwarg ? node(kid(n, at)) : value_none() };
        if (!ok || !set(o.v, "kwarg", kw.v))
            return Value();
        at += kwarg;
        if (!set_list(o.v, "defaults", n, at, n.d))
            return Value();
        return o.v;
    }

    case Nd::Arg: {
        Root o{ make("arg", i) };
        Root nm{ ident_of(ast, i) }, an{ node(n.a) };
        if (!ok || !set(o.v, "arg", nm.v) || !set(o.v, "annotation", an.v))
            return Value();
        return o.v;
    }

    case Nd::Keyword: {
        Root o{ make("keyword", i) };
        Root nm{ n.flags ? ident_of(ast, i) : value_none() };
        Root v{ node(n.a) };
        if (!ok || nm.v.is_nil() || !set(o.v, "arg", nm.v) || !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::Alias: {
        Root o{ make("alias", i) };
        Root nm{ n.b > n.tok + 1 ? dotted_of(ast, n.tok, n.b)
                 : n.flags       ? str_new(Str("*"))
                                 : ident_of(ast, i) };
        Root as{ n.a ? str_new(ast->lex.text_of(ast->lex.tokens[n.a - 1])) : value_none() };
        if (!ok || nm.v.is_nil() || as.v.is_nil() || !set(o.v, "name", nm.v) ||
            !set(o.v, "asname", as.v))
            return Value();
        return o.v;
    }

    case Nd::WithItem: {
        Root o{ make("withitem", 0) };
        Root c{ node(n.a) }, v{ node(n.b) };
        if (!ok || !retarget(v.v, 1) || !set(o.v, "context_expr", c.v) ||
            !set(o.v, "optional_vars", v.v))
            return Value();
        return o.v;
    }

    case Nd::Match: {
        Root o{ make("Match", i) };
        Root s{ node(n.a) };
        if (!ok || !set(o.v, "subject", s.v) || !set_list(o.v, "cases", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::MatchCase: {
        Root o{ make("match_case", 0) };
        Root p{ node(n.a) }, g{ node(n.b) };
        if (!ok || !set(o.v, "pattern", p.v) || !set(o.v, "guard", g.v) ||
            !set_list(o.v, "body", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::MatchValue: {
        Root o{ make("MatchValue", i) };
        Root v{ node(n.a) };
        if (!ok || !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::MatchSingleton: {
        Root o{ make("MatchSingleton", i) };
        Root v{ constant_of(n) };
        if (!ok || v.v.is_nil() || !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::MatchSequence:
    case Nd::MatchOr: {
        Root o{ make(n.kind == Nd::MatchSequence ? Str("MatchSequence") : Str("MatchOr"), i) };
        if (!ok || !set_list(o.v, "patterns", n, 0, n.nkid))
            return Value();
        return o.v;
    }

    case Nd::MatchMapping: {
        Root o{ make("MatchMapping", i) };
        Root rest{ (n.flags & 1) ? ident_of(ast, i) : value_none() };
        if (!ok || rest.v.is_nil() || !set_list(o.v, "keys", n, 0, n.a) ||
            !set_list(o.v, "patterns", n, n.a, n.nkid - n.a) || !set(o.v, "rest", rest.v))
            return Value();
        return o.v;
    }

    case Nd::MatchClass: {
        Root o{ make("MatchClass", i) };
        Root c{ node(n.a) };
        Root names{ obj_value(list_new()) }, pats{ obj_value(list_new()) };
        if (!ok || names.v.is_nil() || pats.v.is_nil() || !set(o.v, "cls", c.v) ||
            !set_list(o.v, "patterns", n, 0, n.b))
            return Value();
        for (u32 k = n.b; k < n.nkid; k++) {
            u32 w = kid(n, k);
            Root nm{ ident_of(ast, w) };
            Root p{ node(ast->at(w).a) };
            if (!ok || nm.v.is_nil() || !list_push(list_at(names.v), nm.v) ||
                !list_push(list_at(pats.v), p.v))
                return fail();
        }
        if (!set(o.v, "kwd_attrs", names.v) || !set(o.v, "kwd_patterns", pats.v))
            return Value();
        return o.v;
    }

    case Nd::MatchStar: {
        Root o{ make("MatchStar", i) };
        Root nm{ (n.flags & 1) ? ident_of(ast, i) : value_none() };
        if (!ok || nm.v.is_nil() || !set(o.v, "name", nm.v))
            return Value();
        return o.v;
    }

    case Nd::MatchAs: {
        Root o{ make("MatchAs", i) };
        Root p{ node(n.a) };
        Root nm{ (n.flags & 1) ? ident_of(ast, i) : value_none() };
        if (!ok || nm.v.is_nil() || !set(o.v, "pattern", p.v) || !set(o.v, "name", nm.v))
            return Value();
        return o.v;
    }

    case Nd::TypeAlias: {
        Root o{ make("TypeAlias", i) };
        Root nm{ node(n.a) }, v{ node(n.b) };
        if (!ok || !retarget(nm.v, 1) || !set(o.v, "name", nm.v) ||
            !set_list(o.v, "type_params", n, 0, n.c) || !set(o.v, "value", v.v))
            return Value();
        return o.v;
    }

    case Nd::TypeVar: {
        Root o{ make("TypeVar", i) };
        Root nm{ ident_of(ast, i) }, b{ node(n.a) }, d{ node(n.b) };
        if (!ok || !set(o.v, "name", nm.v) || !set(o.v, "bound", b.v) ||
            !set(o.v, "default_value", d.v))
            return Value();
        return o.v;
    }

    case Nd::ParamSpec:
    case Nd::TypeVarTuple: {
        Root o{ make(n.kind == Nd::ParamSpec ? Str("ParamSpec") : Str("TypeVarTuple"), i) };
        Root nm{ ident_of(ast, i) }, d{ node(n.b) };
        if (!ok || !set(o.v, "name", nm.v) || !set(o.v, "default_value", d.v))
            return Value();
        return o.v;
    }

    default:
        break;
    }
    Buf<64> b;
    b.put("this node is not in _ast yet: ").put(nd_name(n.kind));
    err_set("SystemError", b.str());
    return fail();
}

} // namespace

// ------------------------------------------------------------------ install

Value ast_tree(Ast &ast, bool interactive, bool eval)
{
    if (!home)
        return err_set("SystemError", "_ast is not installed"), Value();
    if (!ast_spans(ast))
        return Value();
    Builder b{ &ast };
    const Node &root = ast.at(ast.root);

    if (!interactive && !eval)
        return b.node(ast.root);

    Root o{ b.make(eval ? Str("Expression") : Str("Interactive"), 0) };
    if (!b.ok)
        return Value();
    if (interactive)
        return b.set_list(o.v, "body", root, 0, root.nkid) ? o.v : Value();

    // Eval mode: the one expression the module holds.
    if (root.nkid != 1 || ast.at(ast.kids[root.kid0]).kind != Nd::Expr)
        return err_set_at("SyntaxError", "invalid syntax", 1, 0), Value();
    Root e{ b.node(ast.at(ast.kids[root.kid0]).a) };
    if (!b.ok)
        return Value();
    return b.set(o.v, "body", e.v) ? o.v : Value();
}

bool ast_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return err_set("MemoryError", "out of memory") == R::Ok;
        for (usize i = 0; i <= NCLASSES; i++)
            home->cls[i] = Value();
        for (usize i = 0; i < 3; i++)
            home->ctx[i] = Value();
        for (usize i = 0; i < 32; i++)
            home->ops[i] = Value();
        home->module = Value();
        gc_root_hook(home_mark);
    }
    home->module = rd.v;

    // The root, with the constructor every node shares.
    if (home->cls[NCLASSES].is_nil()) {
        Root d{ obj_value(dict_new()) };
        Root nm{ obj_value(str_intern("AST")) };
        Root bases{ obj_value(tuple_new(0)) };
        Root init{ native_new("__init__", n_ast_init) };
        Root rep{ native_new("__repr__", n_ast_repr) };
        StrObj *rk2 = str_intern("__repr__");
        StrObj *ik = str_intern("__init__");
        StrObj *fk = str_intern("_fields");
        StrObj *ak = str_intern("_attributes");
        Root empty{ obj_value(tuple_new(0)) };
        if (d.v.is_nil() || nm.v.is_nil() || bases.v.is_nil() || init.v.is_nil() || !ik || !fk ||
            !ak || !rk2 || rep.v.is_nil() || empty.v.is_nil())
            return oom() == R::Ok;
        if (dict_set(dict_at(d.v), obj_value(rk2), rep.v) != R::Ok ||
            dict_set(dict_at(d.v), obj_value(ik), init.v) != R::Ok ||
            dict_set(dict_at(d.v), obj_value(fk), empty.v) != R::Ok ||
            dict_set(dict_at(d.v), obj_value(ak), empty.v) != R::Ok)
            return false;
        Value made = type_new(nm.v, bases.v, d.v);
        if (made.is_nil())
            return false;
        home->cls[NCLASSES] = made;

        for (usize i = 0; i < NCLASSES; i++) {
            Value base = class_by_name(CLASSES[i].base);
            if (base.is_nil())
                base = home->cls[NCLASSES];
            if (!make_class(i, base))
                return false;
        }
        for (usize i = 0; i < NCLASSES; i++)
            if (!fill_types(i))
                return false;
    }

    // The names the module publishes: every class, then the compile() flags.
    StrObj *rk = str_intern("AST");
    if (!rk || dict_set(dict_at(rd.v), obj_value(rk), home->cls[NCLASSES]) != R::Ok)
        return false;
    for (usize i = 0; i < NCLASSES; i++) {
        StrObj *n = str_intern(CLASSES[i].name);
        if (!n || dict_set(dict_at(rd.v), obj_value(n), home->cls[i]) != R::Ok)
            return false;
    }
    struct Flag {
        Str name;
        i32 v;
    };
    constexpr Flag FLAGS[] = {
        { "PyCF_ONLY_AST", PYCF_ONLY_AST },
        { "PyCF_TYPE_COMMENTS", PYCF_TYPE_COMMENTS },
        { "PyCF_ALLOW_TOP_LEVEL_AWAIT", PYCF_ALLOW_TOP_LEVEL_AWAIT },
        { "PyCF_OPTIMIZED_AST", PYCF_OPTIMIZED_AST },
    };
    for (const Flag &f : FLAGS)
        if (!mod_int(dict_at(rd.v), f.name, f.v))
            return false;
    return true;
}
