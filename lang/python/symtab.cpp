// The scope pass. Two walks: collect, then resolve.
//
// Annotations are not visited, because they are not compiled either; the
// README records that as a difference from CPython.
#include "symtab.h"

#include "err.h"
#include "intern.h"
#include "kernel/fmt.h"

namespace {

// ------------------------------------------------------------------ collect

struct Builder {
    const Ast *ast;
    Symtab *st;
    u32 cur     = 0;
    bool failed = false;

    Scope &scope() { return st->scopes[cur]; }

    bool fail(Str message, u32 n)
    {
        if (!failed) {
            failed         = true;
            const Token &t = ast->lex.tokens[ast->at(n).tok];
            err_set_at("SyntaxError", message, t.line, t.col);
        }
        return false;
    }

    bool oom()
    {
        if (!failed) {
            failed = true;
            err_set("MemoryError", "out of memory");
        }
        return false;
    }

    Sym *note(StrObj *name, u8 flags, u32 node = 0)
    {
        if (!name)
            return oom(), nullptr;
        Vec<Sym> &syms = scope().syms;
        Sym *found     = nullptr;
        for (usize i = 0; i < syms.size(); i++)
            if (syms[i].name == name) {
                syms[i].flags |= flags;
                found = &syms[i];
                break;
            }
        if (!found) {
            Sym s;
            s.name  = name;
            s.flags = flags;
            if (!syms.push(s))
                return oom(), nullptr;
            found = &syms.back();
        }
        if (node && !found->line) {
            const Token &t = ast->lex.tokens[ast->at(node).tok];
            found->line    = t.line;
            found->col     = t.col;
        }
        return found;
    }

    StrObj *name_at(u32 n) { return py_mangle(scope().priv, ast->text(n)); }

    bool note_at(u32 n, u8 flags) { return note(name_at(n), flags, n) != nullptr; }

    // A new scope under the current one, which becomes current.
    u32 open(ScopeKind kind, u32 node)
    {
        Scope s;
        s.kind   = kind;
        s.parent = cur;
        s.node   = node;
        s.priv   = scope().priv;
        if (!st->scopes.push(static_cast<Scope &&>(s)))
            return oom(), 0;
        u32 made          = u32(st->scopes.size() - 1);
        st->at_node[node] = made;
        cur               = made;
        return made;
    }

    // An annotation scope, named by its node and role rather than by the node
    // alone. `sees` is whether it looks in the class it is written in.
    u32 open_anno(u32 node, u8 role, bool sees, Str info)
    {
        Scope s;
        s.kind       = ScopeKind::Annotation;
        s.parent     = cur;
        s.node       = node;
        s.priv       = scope().priv;
        s.sees_class = sees;
        s.info       = info;
        if (!st->scopes.push(static_cast<Scope &&>(s)))
            return oom(), 0;
        u32 made = u32(st->scopes.size() - 1);
        if (!st->annos.push(Anno{ node, role, made }))
            return oom(), 0;
        cur = made;
        if (sees && !note(str_intern("__classdict__"), SF_USE, node))
            return 0;
        return made;
    }

    bool param(StrObj *name)
    {
        Sym *s = note(name, SF_PARAM | SF_ASSIGN);
        if (!s)
            return false;
        if (s->param)
            return fail("duplicate argument in function definition", scope().node);
        s->param = ++scope().nparams;
        return true;
    }

    void kids(u32 n, u32 from, u32 count, void (Builder::*f)(u32))
    {
        const Node &node = ast->at(n);
        for (u32 k = 0; k < count && !failed; k++)
            (this->*f)(ast->kids[node.kid0 + from + k]);
    }

    u32 in_iter = 0; // inside a comprehension's iterable
    u32 in_try  = 0; // inside a try statement

    void stmt(u32 i);
    void expr(u32 i);
    void target(u32 i);
    void iter_names(u32 i);
    void walrus(u32 name);
    void pattern(u32 i);
    void type_params(u32 n, u32 from, u32 count);
    void type_alias(u32 i);
    bool not_in_annotation(u32 i, Str what);
    void comprehension(u32 i);
    void args_outer(u32 i);
    void args_inner(u32 i);
    void function(u32 i);
    void classdef(u32 i);
    bool in_async();
};

// The seven runs of an Arguments node, by the counts it carries.
struct ArgSpan {
    u32 posonly, args, vararg, kwonly, kwdefaults, kwarg, defaults;
    u32 at_posonly, at_args, at_vararg, at_kwonly, at_kwdefaults, at_kwarg, at_defaults;
};

ArgSpan arg_span(const Node &n)
{
    ArgSpan s;
    s.posonly    = n.a;
    s.args       = n.b;
    s.vararg     = (n.flags & ARG_VARARG) ? 1 : 0;
    s.kwonly     = n.c;
    s.kwdefaults = n.c;
    s.kwarg      = (n.flags & ARG_KWARG) ? 1 : 0;
    s.defaults   = n.d;

    s.at_posonly    = 0;
    s.at_args       = s.at_posonly + s.posonly;
    s.at_vararg     = s.at_args + s.args;
    s.at_kwonly     = s.at_vararg + s.vararg;
    s.at_kwdefaults = s.at_kwonly + s.kwonly;
    s.at_kwarg      = s.at_kwdefaults + s.kwdefaults;
    s.at_defaults   = s.at_kwarg + s.kwarg;
    return s;
}

void Builder::args_outer(u32 i)
{
    if (!i)
        return;
    ArgSpan s = arg_span(ast->at(i));
    kids(i, s.at_defaults, s.defaults, &Builder::expr);
    kids(i, s.at_kwdefaults, s.kwdefaults, &Builder::expr);
}

void Builder::args_inner(u32 i)
{
    if (!i)
        return;
    const Node &n = ast->at(i);
    ArgSpan s     = arg_span(n);

    scope().argcount = s.posonly + s.args;
    scope().posonly  = s.posonly;
    scope().kwonly   = s.kwonly;
    scope().varargs  = s.vararg != 0;
    scope().varkw    = s.kwarg != 0;

    // The order a call binds them in, which is the order of varnames:
    // positional, keyword-only, *args, **kwargs.
    const u32 at[4]    = { s.at_posonly, s.at_kwonly, s.at_vararg, s.at_kwarg };
    const u32 count[4] = { s.posonly + s.args, s.kwonly, s.vararg, s.kwarg };
    for (u32 g = 0; g < 4; g++)
        for (u32 k = 0; k < count[g] && !failed; k++) {
            u32 a = ast->kids[n.kid0 + at[g] + k];
            if (!param(name_at(a)))
                return;
        }
}

void Builder::function(u32 i)
{
    const Node &n = ast->at(i);
    // Decorators and defaults belong to the scope the definition sits in.
    kids(i, n.b, n.c, &Builder::expr);
    args_outer(n.a);
    if (n.kind != Nd::Lambda && !note_at(i, SF_ASSIGN))
        return;

    u32 saved = cur;
    if (n.kind != Nd::Lambda && n.pad) {
        // The type parameters get a scope of their own, the function inside
        // it; the defaults are handed in as its arguments.
        bool in_class = scope().kind == ScopeKind::Class;
        if (!open_anno(i, AN_PARAMS, in_class, "the definition of a generic"))
            return;
        const Node &a = ast->at(n.a);
        ArgSpan sp    = arg_span(a);
        bool kwdef    = false;
        for (u32 k = 0; k < sp.kwdefaults; k++)
            kwdef = kwdef || ast->kids[a.kid0 + sp.at_kwdefaults + k] != 0;
        if (sp.defaults && !param(str_intern(".defaults")))
            return;
        if (kwdef && !param(str_intern(".kwdefaults")))
            return;
        scope().argcount = (sp.defaults ? 1 : 0) + (kwdef ? 1 : 0);
        type_params(i, n.b + n.c, n.pad);
        if (failed)
            return;
    }
    if (!open(n.kind == Nd::Lambda ? ScopeKind::Lambda : ScopeKind::Function, i))
        return;
    scope().coroutine = n.kind == Nd::AsyncFunctionDef;
    args_inner(n.a);
    if (n.kind == Nd::Lambda)
        expr(n.b);
    else
        kids(i, 0, n.b, &Builder::stmt);
    if (!failed && scope().coroutine && scope().generator && scope().retval)
        fail("'return' with value in async generator", scope().retval);
    cur = saved;
}

bool Builder::in_async()
{
    return scope().kind == ScopeKind::Function && scope().coroutine;
}

void Builder::classdef(u32 i)
{
    const Node &n = ast->at(i);
    kids(i, n.a + n.b + n.c, n.d, &Builder::expr); // decorators
    if (!note_at(i, SF_ASSIGN))
        return;

    u32 saved = cur;
    if (n.pad) {
        // A generic class is made inside the scope of its type parameters,
        // and so are its bases.
        bool in_class = scope().kind == ScopeKind::Class;
        if (!open_anno(i, AN_PARAMS, in_class, "the definition of a generic"))
            return;
        scope().priv         = str_intern(ast->text(i));
        constexpr Str DOTS[] = { ".type_params", ".generic_base" };
        for (Str dot : DOTS)
            if (!note(str_intern(dot), SF_ASSIGN | SF_USE, i))
                return;
        type_params(i, n.a + n.b + n.c + n.d, n.pad);
    }
    kids(i, 0, n.a, &Builder::expr);   // bases
    kids(i, n.a, n.b, &Builder::expr); // keywords
    if (failed || !open(ScopeKind::Class, i))
        return;
    scope().priv = str_intern(ast->text(i));
    if (n.pad && (!note(str_intern("__type_params__"), SF_ASSIGN, i) ||
                  !note(str_intern(".type_params"), SF_USE, i)))
        return;
    kids(i, n.a + n.b, n.c, &Builder::stmt);
    cur = saved;
}

// The type parameters of node `n`, in the scope just opened for them: each
// name is its own, and each bound and default a scope of its own.
void Builder::type_params(u32 n, u32 from, u32 count)
{
    const Node &parent = ast->at(n);
    for (u32 k = 0; k < count && !failed; k++) {
        u32 tp        = ast->kids[parent.kid0 + from + k];
        const Node &p = ast->at(tp);
        if (ast->text(tp) == "__classdict__") {
            fail("reserved name '__classdict__' cannot be used for type parameter", tp);
            return;
        }
        const Sym *had = st->find(cur, name_at(tp));
        if (had && (had->flags & SF_TPARAM)) {
            Buf<128> b;
            b.put("duplicate type parameter '").put(ast->text(tp)).put("'");
            fail(b.str(), tp);
            return;
        }
        if (!note_at(tp, SF_ASSIGN | SF_TPARAM))
            return;
        bool sees          = scope().sees_class;
        Str bounds         = p.kind == Nd::TypeVar && p.a && ast->at(p.a).kind == Nd::Tuple
                                 ? Str("a TypeVar constraint")
                                 : Str("a TypeVar bound");
        Str dflt           = p.kind == Nd::TypeVar     ? Str("a TypeVar default")
                             : p.kind == Nd::ParamSpec ? Str("a ParamSpec default")
                                                       : Str("a TypeVarTuple default");
        const u8 roles[2]  = { AN_BOUND, AN_DEFAULT };
        const u32 exprs[2] = { p.a, p.b };
        for (u32 r = 0; r < 2 && !failed; r++) {
            if (!exprs[r])
                continue;
            u32 saved = cur;
            if (!open_anno(tp, roles[r], sees, r ? dflt : bounds))
                return;
            expr(exprs[r]);
            cur = saved;
        }
    }
}

// `type X[T] = value`: the name where the statement is, the parameters and
// the value each in a scope of their own.
void Builder::type_alias(u32 i)
{
    const Node &n = ast->at(i);
    target(n.a);
    bool in_class = scope().kind == ScopeKind::Class;
    u32 saved     = cur;
    if (n.c) {
        if (!open_anno(i, AN_PARAMS, in_class, "the definition of a generic"))
            return;
        type_params(i, 0, n.c);
    }
    if (failed || !open_anno(i, AN_VALUE, in_class, "a type alias"))
        return;
    expr(n.b);
    cur = saved;
}

// yield, await and := have nowhere to go in an annotation scope.
bool Builder::not_in_annotation(u32 i, Str what)
{
    if (scope().kind != ScopeKind::Annotation)
        return true;
    Buf<128> b;
    b.put(what).put(" cannot be used within ").put(scope().info);
    return fail(b.str(), i);
}

// The names a comprehension's `for` binds: a `:=` may not rebind one, and one
// may not rebind a `:=` target written before it.
void Builder::iter_names(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    if (n.kind == Nd::Name) {
        // A `:=` in this comprehension got there first.
        const Sym *y = st->find(cur, name_at(i));
        if (y && (y->flags & (SF_GLOBAL | SF_NONLOCAL))) {
            Buf<128> b;
            b.put("comprehension inner loop cannot rebind assignment expression target '");
            b.put(ast->text(i)).put("'");
            fail(b.str(), i);
            return;
        }
        note_at(i, SF_ITER);
    } else if (n.kind == Nd::Starred)
        iter_names(n.a);
    else if (n.kind == Nd::Tuple || n.kind == Nd::List)
        kids(i, 0, n.nkid, &Builder::iter_names);
}

// `name := ...` in a comprehension binds in the scope the comprehension is
// written in (PEP 572), and every comprehension between reaches it through.
void Builder::walrus(u32 name)
{
    StrObj *s = name_at(name);
    if (!s) {
        oom();
        return;
    }
    u32 t = cur;
    for (; st->scopes[t].kind == ScopeKind::Comprehension; t = st->scopes[t].parent) {
        const Sym *y = st->find(t, s);
        if (y && (y->flags & SF_ITER)) {
            Buf<128> b;
            b.put("assignment expression cannot rebind comprehension iteration variable '");
            b.put(ast->text(name)).put("'");
            fail(b.str(), name);
            return;
        }
    }
    if (st->scopes[t].kind == ScopeKind::Class) {
        fail("assignment expression within a comprehension cannot be used in a class body", name);
        return;
    }
    u32 saved = cur;
    cur       = t;
    Sym *y    = note(s, SF_ASSIGN, name);
    bool glob = y && (t == 0 || (y->flags & SF_GLOBAL));
    cur       = saved;
    if (!y)
        return;
    for (u32 c = cur; c != t; c = st->scopes[c].parent) {
        u32 keep = cur;
        cur      = c;
        note(s, glob ? SF_GLOBAL : SF_NONLOCAL, name);
        cur = keep;
    }
}

void Builder::comprehension(u32 i)
{
    const Node &n = ast->at(i);
    // The outermost iterable is evaluated where the comprehension is written.
    in_iter++;
    expr(ast->at(ast->kids[n.kid0]).b);
    in_iter--;

    u32 saved = cur;
    if (!open(ScopeKind::Comprehension, i))
        return;
    scope().argcount  = 1;
    scope().generator = n.kind == Nd::GeneratorExp;
    if (!param(str_intern(".0"))) {
        cur = saved;
        return;
    }
    for (u32 k = 0; k < n.nkid && !failed; k++) {
        u32 c         = ast->kids[n.kid0 + k];
        const Node &g = ast->at(c);
        if (g.flags & 1)
            scope().coroutine = true;
        if (k) {
            in_iter++;
            expr(g.b);
            in_iter--;
        }
        iter_names(g.a);
        target(g.a);
        for (u32 j = 0; j < g.nkid && !failed; j++)
            expr(ast->kids[g.kid0 + j]);
    }
    expr(n.a);
    if (n.kind == Nd::DictComp)
        expr(n.b);
    // A generator expression that awaits is an async generator, and nobody
    // awaits it. Any other comprehension that does is awaited where it is
    // written, which has to be somewhere an await may be.
    bool awaited = scope().coroutine && n.kind != Nd::GeneratorExp;
    cur          = saved;
    if (!awaited || failed)
        return;
    if (scope().kind == ScopeKind::Comprehension)
        scope().coroutine = true;
    else if (!in_async())
        fail("asynchronous comprehension outside of an asynchronous function", i);
}

void Builder::target(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Name:
        note_at(i, SF_ASSIGN);
        return;
    case Nd::Attribute:
        expr(n.a);
        return;
    case Nd::Subscript:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::Starred:
        target(n.a);
        return;
    case Nd::Tuple:
    case Nd::List:
        kids(i, 0, n.nkid, &Builder::target);
        return;
    default:
        expr(i);
        return;
    }
}

void Builder::expr(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Name:
        note_at(i, SF_USE);
        // super() with no arguments reads the class through __class__.
        if (ast->text(i) == "super" && scope().kind != ScopeKind::Module &&
            scope().kind != ScopeKind::Class)
            note(str_intern("__class__"), SF_USE, i);
        return;
    case Nd::Constant:
        return;
    case Nd::Await:
        if (!not_in_annotation(i, "await expression"))
            return;
        if (scope().kind == ScopeKind::Module || scope().kind == ScopeKind::Class) {
            fail("'await' outside function", i);
            return;
        }
        if (scope().kind == ScopeKind::Comprehension)
            scope().coroutine = true;
        else if (!in_async()) {
            fail("'await' outside async function", i);
            return;
        }
        expr(n.a);
        return;

    case Nd::Lambda:
        function(i);
        return;
    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::DictComp:
    case Nd::GeneratorExp:
        comprehension(i);
        return;

    case Nd::Yield:
    case Nd::YieldFrom:
        if (!not_in_annotation(i, "yield expression"))
            return;
        if (scope().kind == ScopeKind::Module || scope().kind == ScopeKind::Class) {
            fail("'yield' outside function", i);
            return;
        }
        if (n.kind == Nd::YieldFrom && in_async()) {
            fail("'yield from' inside async function", i);
            return;
        }
        scope().generator = true;
        expr(n.a);
        return;

    case Nd::NamedExpr:
        if (!not_in_annotation(i, "named expression"))
            return;
        expr(n.b);
        if (in_iter) {
            fail("assignment expression cannot be used in a comprehension iterable expression", i);
            return;
        }
        if (scope().kind == ScopeKind::Comprehension)
            walrus(n.a);
        else
            target(n.a);
        return;
    case Nd::BinOp:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::UnaryOp:
    case Nd::Starred:
        expr(n.a);
        return;
    case Nd::IfExp:
        expr(n.a);
        expr(n.b);
        expr(n.c);
        return;
    case Nd::Compare:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    case Nd::CmpOp:
    case Nd::Keyword:
        expr(n.a);
        return;
    case Nd::Call:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    case Nd::JoinedStr:
    case Nd::TemplateStr:
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    case Nd::FormattedValue:
    case Nd::Interpolation:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::Attribute:
        expr(n.a);
        return;
    case Nd::Subscript:
    case Nd::Slice:
        expr(n.a);
        expr(n.b);
        expr(n.c);
        return;
    case Nd::BoolOp:
    case Nd::Dict:
    case Nd::Set:
    case Nd::List:
    case Nd::Tuple:
        kids(i, 0, n.nkid, &Builder::expr);
        return;
    default:
        return;
    }
}

// What a pattern reads and what it binds.
void Builder::pattern(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::MatchValue:
        expr(n.a);
        return;
    case Nd::MatchSequence:
    case Nd::MatchOr:
        kids(i, 0, n.nkid, &Builder::pattern);
        return;
    case Nd::MatchMapping:
        kids(i, 0, n.a, &Builder::expr);
        kids(i, n.a, n.nkid - n.a, &Builder::pattern);
        if (n.flags & 1)
            note_at(i, SF_ASSIGN);
        return;
    case Nd::MatchClass:
        expr(n.a);
        kids(i, 0, n.b, &Builder::pattern);
        for (u32 k = n.b; k < n.nkid && !failed; k++)
            pattern(ast->at(ast->kids[n.kid0 + k]).a);
        return;
    case Nd::MatchStar:
        if (n.flags & 1)
            note_at(i, SF_ASSIGN);
        return;
    case Nd::MatchAs:
        pattern(n.a);
        if (n.flags & 1)
            note_at(i, SF_ASSIGN);
        return;
    default:
        return;
    }
}

void Builder::stmt(u32 i)
{
    if (!i || failed)
        return;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Module:
        kids(i, 0, n.nkid, &Builder::stmt);
        return;
    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef:
        function(i);
        return;
    case Nd::ClassDef:
        classdef(i);
        return;

    case Nd::Return:
        if (scope().kind != ScopeKind::Function && scope().kind != ScopeKind::Lambda) {
            fail("'return' outside function", i);
            return;
        }
        if (n.a && !scope().retval)
            scope().retval = i;
        expr(n.a);
        return;
    case Nd::Expr:
        expr(n.a);
        return;
    case Nd::Delete:
        for (u32 k = 0; k < n.nkid && !failed; k++) {
            u32 t = ast->kids[n.kid0 + k];
            if (ast->at(t).kind == Nd::Name)
                note_at(t, SF_ASSIGN);
            else
                target(t);
        }
        return;
    case Nd::Assign:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::target);
        return;
    case Nd::AugAssign:
        expr(n.b);
        expr(n.a); // an augmented target is read as well as written
        target(n.a);
        return;
    case Nd::AnnAssign:
        if (n.c) {
            expr(n.c);
            target(n.a);
        } else if (ast->at(n.a).kind != Nd::Name) {
            target(n.a);
        }
        return;
    case Nd::AsyncFor:
        if (!in_async()) {
            fail("'async for' outside async function", i);
            return;
        }
        [[fallthrough]];
    case Nd::For:
        expr(n.b);
        target(n.a);
        kids(i, 0, n.c, &Builder::stmt);
        kids(i, n.c, n.d, &Builder::stmt);
        return;
    case Nd::While:
    case Nd::If:
        expr(n.a);
        kids(i, 0, n.b, &Builder::stmt);
        kids(i, n.b, n.c, &Builder::stmt);
        return;
    case Nd::AsyncWith:
        if (!in_async()) {
            fail("'async with' outside async function", i);
            return;
        }
        [[fallthrough]];
    case Nd::With:
        kids(i, 0, n.a, &Builder::stmt); // WithItem, handled below
        kids(i, n.a, n.b, &Builder::stmt);
        return;
    case Nd::WithItem:
        expr(n.a);
        target(n.b);
        return;
    case Nd::Raise:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::Try:
    case Nd::TryStar:
        in_try++;
        kids(i, 0, n.a, &Builder::stmt);
        kids(i, n.a, n.b, &Builder::stmt);
        kids(i, n.a + n.b, n.c, &Builder::stmt);
        kids(i, n.a + n.b + n.c, n.d, &Builder::stmt);
        in_try--;
        return;
    case Nd::ExceptHandler:
        expr(n.a);
        if (n.flags & 1)
            note_at(i, SF_ASSIGN);
        kids(i, 0, n.b, &Builder::stmt);
        return;
    case Nd::Assert:
        expr(n.a);
        expr(n.b);
        return;
    case Nd::Match:
        expr(n.a);
        kids(i, 0, n.nkid, &Builder::stmt);
        return;
    case Nd::TypeAlias:
        type_alias(i);
        return;
    case Nd::MatchCase:
        pattern(n.a);
        expr(n.b);
        kids(i, 0, n.nkid, &Builder::stmt);
        return;

    case Nd::Import:
    case Nd::ImportFrom:
        if (n.pad & 1) {
            // PEP 810: a lazy import is a module's, and a plain one.
            Str what  = n.kind == Nd::Import ? Str("import") : Str("from ... import");
            Str where = in_try                              ? Str("inside try/except blocks")
                        : scope().kind == ScopeKind::Class  ? Str("inside classes")
                        : scope().kind != ScopeKind::Module ? Str("inside functions")
                                                            : Str();
            if (!where.empty()) {
                Buf<128> b;
                b.put("lazy ").put(what).put(" not allowed ").put(where);
                fail(b.str(), i);
                return;
            }
            if (n.kind == Nd::ImportFrom && n.nkid == 1 && (ast->at(ast->kids[n.kid0]).flags & 1)) {
                fail("lazy from ... import * is not allowed", i);
                return;
            }
        }
        for (u32 k = 0; k < n.nkid && !failed; k++) {
            const Node &a = ast->at(ast->kids[n.kid0 + k]);
            if (a.flags & 1) // `from x import *`
                continue;
            StrObj *bound = py_mangle(scope().priv, a.a ? ast->lex.text_of(ast->lex.tokens[a.a - 1])
                                                        : ast->lex.text_of(ast->lex.tokens[a.tok]));
            if (!note(bound, SF_ASSIGN))
                return;
        }
        return;

    case Nd::Global:
    case Nd::Nonlocal: {
        bool global = n.kind == Nd::Global;
        if (!global && scope().kind == ScopeKind::Module) {
            fail("nonlocal declaration not allowed at module level", i);
            return;
        }
        for (u32 k = 0; k < n.nkid && !failed; k++) {
            u32 t  = ast->kids[n.kid0 + k];
            Sym *s = note(name_at(t), global ? SF_GLOBAL : SF_NONLOCAL, t);
            if (!s)
                return;
            if ((s->flags & SF_GLOBAL) && (s->flags & SF_NONLOCAL)) {
                fail("name is nonlocal and global", t);
                return;
            }
            if (s->flags & SF_PARAM) {
                fail("name is parameter and global", t);
                return;
            }
        }
        return;
    }

    default:
        return;
    }
}

// ------------------------------------------------------------------ resolve

using Names = Vec<StrObj *>;

bool holds(const Names &v, const StrObj *n)
{
    for (usize i = 0; i < v.size(); i++)
        if (v[i] == n)
            return true;
    return false;
}

Sym *find_mut(Scope &s, const StrObj *name)
{
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].name == name)
            return &s.syms[i];
    return nullptr;
}

// A free name in a child: the scope that owns it keeps it in a cell, and every
// scope in between carries it free so the closure reaches through.
bool link_free(Symtab &st, u32 from, StrObj *name)
{
    bool dict = name->str() == "__classdict__";
    bool cls  = name->str() == "__class__";
    for (u32 a = from;; a = st.scopes[a].parent) {
        Sym *y = find_mut(st.scopes[a], name);
        // A class keeps its namespace in a cell for the scopes that see it,
        // and itself in another for its methods.
        if ((dict || cls) && st.scopes[a].kind == ScopeKind::Class) {
            (dict ? st.scopes[a].classdict : st.scopes[a].classcell) = true;
            if (y) {
                y->bind = Bind::Cell;
                return true;
            }
            Sym s;
            s.name = name;
            s.bind = Bind::Cell;
            return st.scopes[a].syms.push(s);
        }
        if (y && (y->bind == Bind::Local || y->bind == Bind::Cell)) {
            y->bind = Bind::Cell;
            return true;
        }
        if (a == 0)
            return true; // the module binds no cells
        if (y) {
            y->bind = Bind::Free;
        } else {
            Sym s;
            s.name = name;
            s.bind = Bind::Free;
            if (!st.scopes[a].syms.push(s))
                return false;
        }
    }
}

bool decide(Symtab &st, u32 si, const Names &bound)
{
    Scope &s   = st.scopes[si];
    bool block = s.kind == ScopeKind::Module || s.kind == ScopeKind::Class;

    for (usize i = 0; i < s.syms.size(); i++) {
        Sym &y = s.syms[i];
        if (y.flags & SF_GLOBAL) {
            y.bind = s.kind == ScopeKind::Module ? Bind::Name : Bind::Global;
        } else if (y.flags & SF_NONLOCAL) {
            if (!holds(bound, y.name)) {
                Buf<160> b;
                b.put("no binding for nonlocal '").put(y.name->str()).put("' found");
                return err_set_at("SyntaxError", b.str(), y.line, y.col), false;
            }
            y.bind = Bind::Free;
        } else if (y.flags & (SF_ASSIGN | SF_PARAM)) {
            y.bind = block ? Bind::Name : Bind::Local;
        } else if (!block && holds(bound, y.name)) {
            y.bind = Bind::Free;
        } else if (s.kind == ScopeKind::Class && holds(bound, y.name)) {
            y.bind = Bind::Free;
        } else {
            y.bind = block ? Bind::Name : Bind::Global;
        }
        // An annotation scope in a class looks in its namespace first.
        if (s.sees_class && y.name->str() != "__classdict__") {
            if (y.bind == Bind::Global && !(y.flags & SF_GLOBAL))
                y.bind = Bind::GlobalOrClass;
            else if (y.bind == Bind::Free)
                y.bind = Bind::FreeOrClass;
        }
    }
    return true;
}

bool name_before(Str a, Str b)
{
    usize n = a.size() < b.size() ? a.size() : b.size();
    for (usize i = 0; i < n; i++)
        if (a[i] != b[i])
            return u8(a[i]) < u8(b[i]);
    return a.size() < b.size();
}

// By name, as CPython orders co_cellvars and co_freevars.
void sort_names(Scope &s, Vec<u32> &v)
{
    for (usize i = 1; i < v.size(); i++)
        for (usize j = i; j && name_before(s.syms[v[j]].name->str(), s.syms[v[j - 1]].name->str());
             j--) {
            u32 t    = v[j];
            v[j]     = v[j - 1];
            v[j - 1] = t;
        }
}

// Parameters first, in the order a call binds them; then the plain locals.
// Cells and free names are sorted.
bool slots(Scope &s)
{
    if (!s.varnames.resize(s.nparams))
        return false;
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].param)
            s.varnames[s.syms[i].param - 1] = u32(i);
    for (usize i = 0; i < s.syms.size(); i++)
        if (!s.syms[i].param && s.syms[i].bind == Bind::Local && !s.varnames.push(u32(i)))
            return false;
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].bind == Bind::Cell && !s.cellvars.push(u32(i)))
            return false;
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].bind == Bind::Free && !s.freevars.push(u32(i)))
            return false;
    sort_names(s, s.cellvars);
    sort_names(s, s.freevars);

    for (usize k = 0; k < s.varnames.size(); k++)
        if (s.syms[s.varnames[k]].bind == Bind::Local)
            s.syms[s.varnames[k]].slot = u32(k);
    for (usize k = 0; k < s.cellvars.size(); k++)
        s.syms[s.cellvars[k]].slot = u32(k);
    for (usize k = 0; k < s.freevars.size(); k++)
        s.syms[s.freevars[k]].slot = u32(k);
    return true;
}

bool analyze(Symtab &st, u32 si, const Names &bound)
{
    if (!decide(st, si, bound))
        return false;

    for (u32 c = si + 1; c < st.scopes.size(); c++) {
        if (st.scopes[c].parent != si)
            continue;

        // What the child may capture: this scope's own bindings on top of
        // what reaches it from further out, less anything declared global. A
        // class body binds nothing a nested scope can see.
        Names next;
        for (usize i = 0; i < bound.size(); i++) {
            Scope &s = st.scopes[si];
            Sym *y   = find_mut(s, bound[i]);
            if (y && (y->flags & SF_GLOBAL))
                continue;
            if (!next.push(bound[i]))
                return err_set("MemoryError", "out of memory"), false;
        }
        if (st.scopes[si].kind == ScopeKind::Class) {
            StrObj *cd = str_intern("__classdict__");
            StrObj *cc = str_intern("__class__");
            if (!cd || (!holds(next, cd) && !next.push(cd)) || !cc ||
                (!holds(next, cc) && !next.push(cc)))
                return err_set("MemoryError", "out of memory"), false;
        } else
            for (usize i = 0; i < st.scopes[si].syms.size(); i++) {
                Sym &y = st.scopes[si].syms[i];
                if (y.bind != Bind::Local && y.bind != Bind::Cell && y.bind != Bind::Free)
                    continue;
                if (holds(next, y.name))
                    continue;
                if (!next.push(y.name))
                    return err_set("MemoryError", "out of memory"), false;
            }

        if (!analyze(st, c, next))
            return false;

        for (usize i = 0; i < st.scopes[c].syms.size(); i++) {
            Sym &y = st.scopes[c].syms[i];
            if (y.bind == Bind::Free && !link_free(st, si, y.name))
                return err_set("MemoryError", "out of memory"), false;
        }
    }

    return slots(st.scopes[si]);
}

} // namespace

u32 Symtab::anno(u32 node, u8 role) const
{
    for (usize i = 0; i < annos.size(); i++)
        if (annos[i].node == node && annos[i].role == role)
            return annos[i].scope;
    return 0;
}

const Sym *Symtab::find(u32 scope, const StrObj *name) const
{
    const Scope &s = scopes[scope];
    for (usize i = 0; i < s.syms.size(); i++)
        if (s.syms[i].name == name)
            return &s.syms[i];
    return nullptr;
}

bool Symtab::build(const Ast &ast)
{
    if (!at_node.resize(ast.nodes.size()))
        return err_set("MemoryError", "out of memory"), false;

    Scope module;
    module.node = ast.root;
    if (!scopes.push(static_cast<Scope &&>(module)))
        return err_set("MemoryError", "out of memory"), false;
    at_node[ast.root] = 0;

    Builder b{ &ast, this };
    b.stmt(ast.root);
    if (b.failed)
        return false;

    Names empty;
    return analyze(*this, 0, empty);
}
