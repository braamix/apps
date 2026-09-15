// --dump-ast: the tree as indented lines.
//
// The format is the contract tools/mkast.py writes to, so what this prints can
// be compared with what CPython's own ast module built for the same source.
// A node line is `Kind` and its inline scalars; a field is `name:` with its
// children indented under it, `name: -` when absent and `name: []` when empty.
#include "err.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "parse.h"

namespace {

struct Dumper {
    const Ast *ast;
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

    void indent(u32 n)
    {
        for (u32 k = 0; k < n * 2; k++)
            put(' ');
    }

    Str text(u32 n) { return ast->text(n); }

    // A dotted name, kept as the span of tokens it was written as.
    void dotted(u32 from, u32 to)
    {
        for (u32 k = from; k < to; k++) {
            const Token &t = ast->lex.tokens[k];
            if (t.kind == Tok::Name)
                put(ast->lex.text_of(t));
            else
                put('.');
        }
    }

    u32 kid(const Node &n, usize k) const { return ast->kids[n.kid0 + k]; }

    void field(u32 d, Str name, u32 child)
    {
        indent(d);
        put(name);
        if (!child) {
            put(": -\n");
            return;
        }
        put(":\n");
        node(d + 1, child);
    }

    void list_head(u32 d, Str name, usize count)
    {
        indent(d);
        put(name);
        put(count ? ":\n" : ": []\n");
    }

    void slice(u32 d, Str name, const Node &n, usize from, usize count)
    {
        list_head(d, name, count);
        for (usize k = 0; k < count; k++)
            node(d + 1, kid(n, from + k));
    }

    void constant(const Node &n);
    void node(u32 d, u32 i);
};

void Dumper::constant(const Node &n)
{
    const Token &t = ast->lex.tokens[n.tok];
    switch (Const(n.flags)) {
    case Const::None:
        put("None");
        return;
    case Const::True:
        put("True");
        return;
    case Const::False:
        put("False");
        return;
    case Const::Ellipsis:
        put("Ellipsis");
        return;
    case Const::Int: {
        Buf<24> b;
        i64 v = t.ival;
        if (v < 0) {
            b.put('-');
            v = -v;
        }
        b.put(u64(v));
        put(b.str());
        return;
    }
    case Const::Float: {
        char tmp[48];
        put(float_text(tmp, sizeof tmp, t.fval));
        return;
    }
    case Const::Str:
    case Const::Bytes: {
        Str body = ast->lex.text_of(t);
        Root v{ Const(n.flags) == Const::Bytes ? bytes_new(body) : obj_value(str_raw(body)) };
        if (v.v.is_nil() || py_repr(v.v, *out) != R::Ok)
            ok = false;
        return;
    }
    }
}

void Dumper::node(u32 d, u32 i)
{
    if (!ok)
        return;
    const Node &n = ast->at(i);
    indent(d);
    put(nd_name(n.kind));

    switch (n.kind) {
    case Nd::Name:
    case Nd::Attribute:
    case Nd::Arg:
    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef:
    case Nd::ClassDef:
        put(' ');
        put(text(i));
        break;
    case Nd::Constant:
        put(' ');
        constant(n);
        break;
    case Nd::FString: {
        put(' ');
        Root v{ obj_value(str_raw(text(i))) };
        if (v.v.is_nil() || py_repr(v.v, *out) != R::Ok)
            ok = false;
        break;
    }
    case Nd::BinOp:
        put(' ');
        put(op_symbol(Op(n.flags)));
        break;
    case Nd::AugAssign:
        put(' ');
        put(op_symbol(Op(n.flags)));
        put('=');
        break;
    case Nd::UnaryOp: {
        constexpr Str UN[] = { "~", "not", "+", "-" };
        put(' ');
        put(UN[n.flags & 3]);
        break;
    }
    case Nd::BoolOp:
        put(Bool(n.flags) == Bool::And ? " and" : " or");
        break;
    case Nd::CmpOp:
        put(' ');
        put(cmp_symbol(Cmp(n.flags)));
        break;
    case Nd::Keyword:
        put(' ');
        put(n.flags ? text(i) : Str("**"));
        break;
    case Nd::Alias:
        put(' ');
        if (n.b > n.tok + 1) {
            dotted(n.tok, n.b);
        } else if (n.flags) {
            put('*');
        } else {
            put(text(i));
        }
        if (n.a) {
            put(" as ");
            put(ast->lex.text_of(ast->lex.tokens[n.a - 1]));
        }
        break;
    case Nd::ImportFrom: {
        put(' ');
        if (n.flags)
            dotted(n.tok, n.b);
        else
            put('-');
        Buf<16> b;
        b.put(" level=").put(u64(n.a));
        put(b.str());
        break;
    }
    case Nd::AnnAssign:
        if (n.flags & 1)
            put(" simple");
        break;
    case Nd::Comprehen:
        if (n.flags & 1)
            put(" async");
        break;
    case Nd::ExceptHandler:
        if (n.flags & 1) {
            put(' ');
            put(text(i));
        }
        break;
    default:
        break;
    }
    put('\n');

    switch (n.kind) {
    case Nd::Module:
        slice(d + 1, "body", n, 0, n.nkid);
        break;
    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef:
        field(d + 1, "args", n.a);
        slice(d + 1, "body", n, 0, n.b);
        slice(d + 1, "decorators", n, n.b, n.c);
        field(d + 1, "returns", n.d);
        break;
    case Nd::ClassDef:
        slice(d + 1, "bases", n, 0, n.a);
        slice(d + 1, "keywords", n, n.a, n.b);
        slice(d + 1, "body", n, n.a + n.b, n.c);
        slice(d + 1, "decorators", n, n.a + n.b + n.c, n.d);
        break;
    case Nd::Return:
    case Nd::Yield:
        field(d + 1, "value", n.a);
        break;
    case Nd::Expr:
    case Nd::Await:
    case Nd::YieldFrom:
    case Nd::Starred:
        field(d + 1, "value", n.a);
        break;
    case Nd::Delete:
        slice(d + 1, "targets", n, 0, n.nkid);
        break;
    case Nd::Global:
    case Nd::Nonlocal:
        slice(d + 1, "names", n, 0, n.nkid);
        break;
    case Nd::Import:
    case Nd::ImportFrom:
        slice(d + 1, "names", n, 0, n.nkid);
        break;
    case Nd::Assign:
        slice(d + 1, "targets", n, 0, n.nkid);
        field(d + 1, "value", n.a);
        break;
    case Nd::AugAssign:
    case Nd::NamedExpr:
        field(d + 1, "target", n.a);
        field(d + 1, "value", n.b);
        break;
    case Nd::AnnAssign:
        field(d + 1, "target", n.a);
        field(d + 1, "annotation", n.b);
        field(d + 1, "value", n.c);
        break;
    case Nd::For:
    case Nd::AsyncFor:
        field(d + 1, "target", n.a);
        field(d + 1, "iter", n.b);
        slice(d + 1, "body", n, 0, n.c);
        slice(d + 1, "orelse", n, n.c, n.d);
        break;
    case Nd::While:
    case Nd::If:
        field(d + 1, "test", n.a);
        slice(d + 1, "body", n, 0, n.b);
        slice(d + 1, "orelse", n, n.b, n.c);
        break;
    case Nd::With:
    case Nd::AsyncWith:
        slice(d + 1, "items", n, 0, n.a);
        slice(d + 1, "body", n, n.a, n.b);
        break;
    case Nd::Raise:
        field(d + 1, "exc", n.a);
        field(d + 1, "cause", n.b);
        break;
    case Nd::Try:
        slice(d + 1, "body", n, 0, n.a);
        slice(d + 1, "handlers", n, n.a, n.b);
        slice(d + 1, "orelse", n, n.a + n.b, n.c);
        slice(d + 1, "finalbody", n, n.a + n.b + n.c, n.d);
        break;
    case Nd::Assert:
        field(d + 1, "test", n.a);
        field(d + 1, "msg", n.b);
        break;
    case Nd::BoolOp:
        slice(d + 1, "values", n, 0, n.nkid);
        break;
    case Nd::BinOp:
        field(d + 1, "left", n.a);
        field(d + 1, "right", n.b);
        break;
    case Nd::UnaryOp:
        field(d + 1, "operand", n.a);
        break;
    case Nd::Lambda:
        field(d + 1, "args", n.a);
        field(d + 1, "body", n.b);
        break;
    case Nd::IfExp:
        field(d + 1, "test", n.a);
        field(d + 1, "body", n.b);
        field(d + 1, "orelse", n.c);
        break;
    case Nd::Dict: {
        usize pairs = n.nkid / 2;
        list_head(d + 1, "keys", pairs);
        for (usize k = 0; k < pairs; k++) {
            u32 key = kid(n, k * 2);
            if (key)
                node(d + 2, key);
            else {
                indent(d + 2);
                put("-\n");
            }
        }
        list_head(d + 1, "values", pairs);
        for (usize k = 0; k < pairs; k++)
            node(d + 2, kid(n, k * 2 + 1));
        break;
    }
    case Nd::Set:
    case Nd::List:
    case Nd::Tuple:
        slice(d + 1, "elts", n, 0, n.nkid);
        break;
    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::GeneratorExp:
        field(d + 1, "elt", n.a);
        slice(d + 1, "generators", n, 0, n.nkid);
        break;
    case Nd::DictComp:
        field(d + 1, "key", n.a);
        field(d + 1, "value", n.b);
        slice(d + 1, "generators", n, 0, n.nkid);
        break;
    case Nd::Compare:
        field(d + 1, "left", n.a);
        slice(d + 1, "ops", n, 0, n.nkid);
        break;
    case Nd::CmpOp:
        field(d + 1, "operand", n.a);
        break;
    case Nd::Call:
        field(d + 1, "func", n.a);
        slice(d + 1, "args", n, 0, n.b);
        slice(d + 1, "keywords", n, n.b, n.nkid - n.b);
        break;
    case Nd::Attribute:
        field(d + 1, "value", n.a);
        break;
    case Nd::Subscript:
        field(d + 1, "value", n.a);
        field(d + 1, "slice", n.b);
        break;
    case Nd::Slice:
        field(d + 1, "lower", n.a);
        field(d + 1, "upper", n.b);
        field(d + 1, "step", n.c);
        break;
    case Nd::Comprehen:
        field(d + 1, "target", n.a);
        field(d + 1, "iter", n.b);
        slice(d + 1, "ifs", n, 0, n.nkid);
        break;
    case Nd::ExceptHandler:
        field(d + 1, "type", n.a);
        slice(d + 1, "body", n, 0, n.b);
        break;
    case Nd::Arguments: {
        usize at = 0;
        slice(d + 1, "posonly", n, at, n.a);
        at += n.a;
        slice(d + 1, "args", n, at, n.b);
        at += n.b;
        field(d + 1, "vararg", (n.flags & ARG_VARARG) ? kid(n, at) : 0);
        at += (n.flags & ARG_VARARG) ? 1 : 0;
        slice(d + 1, "kwonly", n, at, n.c);
        at += n.c;
        list_head(d + 1, "kw_defaults", n.c);
        for (u32 k = 0; k < n.c; k++) {
            u32 v = kid(n, at + k);
            if (v)
                node(d + 2, v);
            else {
                indent(d + 2);
                put("-\n");
            }
        }
        at += n.c;
        field(d + 1, "kwarg", (n.flags & ARG_KWARG) ? kid(n, at) : 0);
        at += (n.flags & ARG_KWARG) ? 1 : 0;
        slice(d + 1, "defaults", n, at, n.d);
        break;
    }
    case Nd::Arg:
        field(d + 1, "annotation", n.a);
        break;
    case Nd::Keyword:
        field(d + 1, "value", n.a);
        break;
    case Nd::WithItem:
        field(d + 1, "context", n.a);
        field(d + 1, "vars", n.b);
        break;
    default:
        break;
    }
}

} // namespace

Str nd_name(Nd k)
{
    switch (k) {
#define NAME(x) \
    case Nd::x: \
        return #x
        NAME(Nop);
        NAME(Module);
        NAME(FunctionDef);
        NAME(AsyncFunctionDef);
        NAME(ClassDef);
        NAME(Return);
        NAME(Delete);
        NAME(Assign);
        NAME(AugAssign);
        NAME(AnnAssign);
        NAME(For);
        NAME(AsyncFor);
        NAME(While);
        NAME(If);
        NAME(With);
        NAME(AsyncWith);
        NAME(Raise);
        NAME(Try);
        NAME(Assert);
        NAME(Import);
        NAME(ImportFrom);
        NAME(Global);
        NAME(Nonlocal);
        NAME(Expr);
        NAME(Pass);
        NAME(Break);
        NAME(Continue);
        NAME(BoolOp);
        NAME(NamedExpr);
        NAME(BinOp);
        NAME(UnaryOp);
        NAME(Lambda);
        NAME(IfExp);
        NAME(Dict);
        NAME(Set);
        NAME(ListComp);
        NAME(SetComp);
        NAME(DictComp);
        NAME(GeneratorExp);
        NAME(Await);
        NAME(Yield);
        NAME(YieldFrom);
        NAME(Compare);
        NAME(Call);
        NAME(FString);
        NAME(Constant);
        NAME(Attribute);
        NAME(Subscript);
        NAME(Starred);
        NAME(Name);
        NAME(List);
        NAME(Tuple);
        NAME(Slice);
        NAME(CmpOp);
        NAME(Comprehen);
        NAME(ExceptHandler);
        NAME(Arguments);
        NAME(Arg);
        NAME(Keyword);
        NAME(Alias);
        NAME(WithItem);
#undef NAME
    }
    return "?";
}

bool ast_dump(Str source, String &out)
{
    Ast ast;
    if (!ast.parse(source))
        return false;
    Dumper d{ &ast, &out };
    d.node(0, ast.root);
    return d.ok;
}
