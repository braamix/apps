// The emitter: one code object per scope, jumps patched in place, and the
// block structure `break`, `continue` and `return` unwind through.
//
// There is no block stack for loops: an exit out of a `try`/`finally` or a
// `with` emits that cleanup inline before it jumps, which is what CPython has
// done since 3.9 and what keeps the VM's block stack to exception handlers
// alone.
#include "compile.h"

#include "annot.h"
#include "bigint.h"
#include "complex.h"
#include "err.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "ops.h"
#include "symtab.h"
#include "typevar.h"

namespace {

u32 opt_default  = 0;
i32 opt_override = -1;

u32 level()
{
    return opt_override >= 0 ? u32(opt_override) : opt_default;
}

// What an exit has to walk back out of.
enum class FK : u8 {
    Loop,     // break and continue land here
    Try,      // a `try` body with handlers: the block has to be popped
    Finally,  // a `finally` clause to run on the way out
    With,     // a context manager to call __exit__ on
    Handler,  // an `except` clause in progress
    StarBody, // the except* clauses, which nothing may leave early
};

struct FBlock {
    FK kind      = FK::Loop;
    u32 cont     = 0;       // Loop: where `continue` jumps
    u32 pops     = 0;       // Loop: values the loop itself left on the stack
    u32 node     = 0;       // Finally: the Try node, for the inline copy
    u32 held     = 0;       // what `pending` was when the block was opened
    StrObj *name = nullptr; // Handler: the name `as` bound, or null
    bool busy    = false;   // Finally: its copy is being emitted right now
    bool async   = false;   // With: `async with`, whose __aexit__ is awaited
    Vec<u32> breaks;
};

enum : u8 { NO_ROLE = 0xff };

// One code object under construction. The Root is why every nested scope is
// compiled from its own C++ frame.
struct Unit {
    Unit *prev = nullptr;
    u32 scope  = 0;
    Root code;
    Vec<FBlock> blocks;
    u32 line       = 0;
    u32 blocks_max = 0;    // the deepest the run-time block stack goes
    u32 pending    = 0;    // return values sitting under an inlined finally body
    Vec<StrObj *> statics; // a class: the X of every `self.X = ...` under it
    u8 role = NO_ROLE;     // an annotation scope's AN_*
};

// Byte order, which is codepoint order for UTF-8.
bool str_before(Str a, Str b)
{
    for (usize i = 0; i < a.size() && i < b.size(); i++)
        if (a[i] != b[i])
            return u8(a[i]) < u8(b[i]);
    return a.size() < b.size();
}

// What a pattern being compiled has on the stack, and where its failures go:
// CPython's pattern_context. Every pattern consumes the subject on top. The
// names it captures wait on the stack, under `on_top` working values, until
// the whole pattern has matched, and a failure jumps to the entry of
// `fail_pop` that pops what is there.
struct PatCtx {
    Vec<StrObj *> stores;
    Vec<Vec<u32>> fail_pop;
    u32 on_top             = 0;
    bool allow_irrefutable = false;
};

// The three opcodes a name binding answers.
struct NameOps {
    Bc load, store, del;
    u32 arg;
};

struct Compiler {
    const Ast *ast = nullptr;
    Symtab st;
    Root filename;
    Unit *u        = nullptr;
    bool failed    = false;
    bool print_top = false; // Single mode: a top-level statement is printed

    CodeObj *co() { return code_of(u->code.v); }

    const Scope &scope() const { return st.scopes[u->scope]; }

    u32 line_of(u32 node) const { return ast->lex.tokens[ast->at(node).tok].line; }

    bool oom()
    {
        if (!failed) {
            failed = true;
            err_set("MemoryError", "out of memory");
        }
        return false;
    }

    bool fail(Str message, u32 node)
    {
        if (!failed) {
            failed         = true;
            const Token &t = ast->lex.tokens[ast->at(node).tok];
            err_set_at("SyntaxError", message, t.line, t.col);
        }
        return false;
    }

    // ------------------------------------------------------------- emitting

    u32 here() { return u32(co()->code.size()); }

    bool emit(Bc op, u32 arg, u32 node)
    {
        if (failed)
            return false;
        u32 line = node ? line_of(node) : u->line;
        if (line && line != u->line) {
            u->line = line;
            if (!co()->lines.push(LineEntry{ here(), line }))
                return oom();
        }
        return co()->code.push(Instr{ op, arg }) ? true : oom();
    }

    bool emit(Bc op, u32 node) { return emit(op, 0, node); }

    // The index to patch once the target is known.
    u32 emit_jump(Bc op, u32 node)
    {
        u32 at = here();
        return emit(op, 0, node) ? at : 0;
    }

    void patch(u32 at)
    {
        if (!failed && at < co()->code.size())
            co()->code[at].arg = here();
    }

    void patch_to(u32 at, u32 target)
    {
        if (!failed && at < co()->code.size())
            co()->code[at].arg = target;
    }

    void patch_all(const Vec<u32> &v)
    {
        for (usize k = 0; k < v.size(); k++)
            patch(v[k]);
    }

    // -------------------------------------------------------------- pooling

    // CPython interns a constant made of name characters, and the one-letter
    // Latin-1 strings are its singletons; marshal tells them apart.
    static Value intern_const(Value v)
    {
        if (!is_str(v) || type_of(v) != &str_type)
            return v;
        StrObj *s  = str_of(v);
        Str b      = s->str();
        bool names = true;
        for (usize i = 0; i < b.size() && names; i++) {
            char c = b[i];
            names  = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                     c == '_';
        }
        bool latin1 = s->chars == 1 && b.size() <= 2 && u8(b[0]) <= 0xc3;
        if (!names && !latin1)
            return v;
        StrObj *o = str_intern(b);
        return o ? obj_value(o) : v;
    }

    u32 add_const(Value v)
    {
        if (failed || v.is_nil())
            return oom(), 0;
        v = intern_const(v);
        if (is_tuple(v)) {
            TupleObj *t = static_cast<TupleObj *>(v.obj());
            for (u32 i = 0; i < t->len; i++) {
                Root keep{ v };
                Value m = intern_const(static_cast<TupleObj *>(keep.v.obj())->items()[i]);
                static_cast<TupleObj *>(keep.v.obj())->items()[i] = m;
            }
        }
        Root r{ v };
        Vec<Value> &c = co()->consts;
        if (!is_code(v))
            for (usize i = 0; i < c.size(); i++) {
                if (is_code(c[i]) || type_of(c[i]) != type_of(v))
                    continue;
                bool same = false;
                if (py_eq(c[i], v, same) == R::Ok && same)
                    return u32(i);
            }
        if (!c.push(v))
            return oom(), 0;
        return u32(c.size() - 1);
    }

    u32 const_none() { return add_const(value_none()); }

    u32 pool(Vec<Value> &v, StrObj *s)
    {
        if (failed || !s)
            return oom(), 0;
        for (usize i = 0; i < v.size(); i++)
            if (v[i].obj() == static_cast<Obj *>(s))
                return u32(i);
        if (!v.push(obj_value(s)))
            return oom(), 0;
        return u32(v.size() - 1);
    }

    u32 name_index(StrObj *s) { return pool(co()->names, s); }

    // A name as the scope sees it: `__x` in a class is `_C__x`.
    StrObj *ident(u32 node) { return mangled(ast->text(node)); }

    StrObj *mangled(Str name) { return py_mangle(scope().priv, name); }

    // A name that is never mangled: a keyword argument, an attribute noted
    // for __static_attributes__, a class's own __name__.
    StrObj *raw_ident(u32 node) { return str_intern(ast->text(node)); }

    // --------------------------------------------------------------- names

    NameOps name_ops(StrObj *s)
    {
        const Sym *y = st.find(u->scope, s);
        Bind b       = y ? y->bind : Bind::Name;
        // A class body names its own __class__ cell only to hand it on.
        if (scope().kind == ScopeKind::Class && b == Bind::Cell && s->str() == "__class__")
            b = Bind::Name;
        switch (b) {
        case Bind::Local:
            return { Bc::LoadFast, Bc::StoreFast, Bc::DeleteFast, y->slot };
        case Bind::Cell:
            return { Bc::LoadDeref, Bc::StoreDeref, Bc::DeleteDeref, y->slot };
        case Bind::Free:
            return { Bc::LoadDeref, Bc::StoreDeref, Bc::DeleteDeref,
                     u32(co()->cellvars.size()) + y->slot };
        case Bind::Global:
        case Bind::GlobalOrClass:
            return { Bc::LoadGlobal, Bc::StoreGlobal, Bc::DeleteGlobal, name_index(s) };
        case Bind::FreeOrClass:
            return { Bc::LoadDeref, Bc::StoreDeref, Bc::DeleteDeref,
                     u32(co()->cellvars.size()) + y->slot };
        case Bind::Name:
            break;
        }
        return { Bc::LoadName, Bc::StoreName, Bc::DeleteName, name_index(s) };
    }

    // The cell holding the class namespace an annotation scope looks in.
    u32 classdict_slot()
    {
        const Sym *y = st.find(u->scope, str_intern("__classdict__"));
        return y ? u32(co()->cellvars.size()) + y->slot : 0;
    }

    bool load_name(StrObj *s, u32 node)
    {
        const Sym *y = st.find(u->scope, s);
        if (y && y->bind == Bind::GlobalOrClass)
            return emit(Bc::LoadDeref, classdict_slot(), node) &&
                   emit(Bc::LoadFromDictOrGlobals, name_index(s), node);
        if (y && y->bind == Bind::FreeOrClass)
            return emit(Bc::LoadDeref, classdict_slot(), node) &&
                   emit(Bc::LoadFromDictOrDeref, u32(co()->cellvars.size()) + y->slot, node);
        NameOps o = name_ops(s);
        return emit(o.load, o.arg, node);
    }

    bool store_name(StrObj *s, u32 node)
    {
        NameOps o = name_ops(s);
        return emit(o.store, o.arg, node);
    }

    bool del_name(StrObj *s, u32 node)
    {
        NameOps o = name_ops(s);
        return emit(o.del, o.arg, node);
    }

    // The cell a nested scope wants, as this scope names it.
    bool load_cell(StrObj *s, u32 node)
    {
        const Sym *y = st.find(u->scope, s);
        if (!y || (y->bind != Bind::Cell && y->bind != Bind::Free && y->bind != Bind::FreeOrClass))
            return fail("a closure wants a name this scope does not bind", node);
        u32 arg = y->bind == Bind::Cell ? y->slot : u32(co()->cellvars.size()) + y->slot;
        return emit(Bc::LoadClosure, arg, node);
    }

    // ----------------------------------------------------------- the visits

    u32 kid(u32 n, u32 k) const { return ast->kids[ast->at(n).kid0 + k]; }

    // Every push goes through here, so blocks_max is the frame's block stack.
    bool block_push(FBlock &f)
    {
        f.held = u->pending;
        if (!u->blocks.push(static_cast<FBlock &&>(f)))
            return oom();
        note_blocks(0);
        return true;
    }

    // A handler the block list does not carry -- the one round an `async for`
    // waiting for its next item -- still needs room at run time.
    void note_blocks(u32 extra)
    {
        u32 n = extra;
        for (usize i = 0; i < u->blocks.size(); i++)
            n += u->blocks[i].kind != FK::Loop ? 1 : 0;
        if (n > u->blocks_max)
            u->blocks_max = n;
    }

    // The value on top is awaited: its iterator is driven to the end, and
    // what it returned is left in its place.
    bool await_top(u32 node, u32 from)
    {
        return emit(Bc::GetAwaitable, from, node) && emit(Bc::LoadConst, const_none(), node) &&
               emit(Bc::YieldFrom, node);
    }

    // The next item of the async iterator on top, under a handler that ends
    // the loop. The handler's index is returned for the caller to patch.
    u32 async_next(u32 node)
    {
        u32 h = emit_jump(Bc::SetupFinally, node);
        note_blocks(1);
        if (!emit(Bc::GetANext, node) || !await_top(node, AW_ANEXT) || !emit(Bc::PopBlock, node))
            return 0;
        return h;
    }

    // The scope being compiled yields into an async generator.
    bool async_gen() const { return scope().coroutine && scope().generator; }

    bool stmts(u32 n, u32 from, u32 count)
    {
        for (u32 k = 0; k < count; k++)
            if (!stmt(kid(n, from + k)))
                return false;
        return true;
    }

    bool exprs(u32 n, u32 from, u32 count)
    {
        for (u32 k = 0; k < count; k++)
            if (!expr(kid(n, from + k)))
                return false;
        return true;
    }

    bool has_star(u32 n, u32 from, u32 count)
    {
        for (u32 k = 0; k < count; k++)
            if (ast->at(kid(n, from + k)).kind == Nd::Starred)
                return true;
        return false;
    }

    bool expr(u32 i);
    bool empty_str(u32 &count, u32 node)
    {
        count++;
        return emit(Bc::LoadConst, add_const(str_new(Str(""))), node);
    }
    Value const_of(u32 i);
    Value literal_of(u32 i);
    bool stmt(u32 i);
    bool store(u32 i);
    bool del(u32 i);

    bool sequence(u32 i, Nd kind);
    bool dict(u32 i);
    bool boolop(u32 i);
    bool compare(u32 i);
    bool call(u32 i);
    bool call_args(u32 i, u32 at, u32 nargs, u32 nkw, u32 pre, u32 node, StrObj *extra = nullptr);
    bool closure_of(u32 node, u32 &flags);
    bool closure_of_scope(u32 scope, u32 node, u32 &flags);
    u32 nested_scope(u32 scope, u32 node, StrObj *name, u8 role);
    bool emit_type_params(u32 n, u32 from, u32 count);
    bool anno_function(u32 node, u8 role, StrObj *name);
    bool anno_body(u32 node, u8 role);
    bool annotate_of(u32 node, u32 &flags);
    bool annotate_prologue(u32 node);
    bool annotate_deferred(u32 node);
    bool annotate_args(u32 node);
    bool annotate_one(u32 arg, u32 &count);
    bool store_annotate(u32 node);
    bool conditional_set(u32 node);
    bool generic_function(u32 i, u32 flags);
    bool generic_class(u32 i);
    bool type_alias(u32 i);
    bool function(u32 i, bool as_statement);
    bool classdef(u32 i);
    bool comprehension(u32 i);
    bool import(u32 i);
    Bc import_op(u32 n, bool star);
    bool import_from(u32 i);
    bool try_stmt(u32 i);
    bool try_except(u32 i);
    bool try_star(u32 i);
    bool with_at(u32 i, u32 k);
    bool unwind(usize down_to, bool preserve_tos);
    bool loop_exit(bool is_break, u32 node);

    bool match_stmt(u32 i);
    bool pattern(u32 p, PatCtx &pc);
    bool subpattern(u32 p, PatCtx &pc);
    bool jump_fail(PatCtx &pc, Bc op, u32 node);
    bool emit_fail_pop(PatCtx &pc);
    bool rotate(u32 count, u32 node);
    bool store_capture(StrObj *name, PatCtx &pc, u32 node);
    bool not_none_or_fail(PatCtx &pc, u32 node);
    bool pattern_sequence(u32 p, PatCtx &pc);
    bool pattern_mapping(u32 p, PatCtx &pc);
    bool pattern_class(u32 p, PatCtx &pc);
    bool pattern_or(u32 p, PatCtx &pc);
    bool pattern_as(u32 p, PatCtx &pc);
    bool is_wildcard(u32 p) const
    {
        return ast->at(p).kind == Nd::MatchAs && !ast->at(p).a && !(ast->at(p).flags & 1);
    }
    bool is_star_wildcard(u32 p) const
    {
        return ast->at(p).kind == Nd::MatchStar && !(ast->at(p).flags & 1);
    }

    Value qualname_of(StrObj *name);
    Value docstring(u32 node);
    bool store_doc();
    bool class_preamble(u32 node);
    bool note_static(u32 attr);
    bool store_statics();
    u32 nested(u32 node);
    bool body_of(u32 node);
};

// ---------------------------------------------------------------- unwinding

bool Compiler::unwind(usize down_to, bool preserve_tos)
{
    for (usize b = u->blocks.size(); b > down_to; b--) {
        FK kind      = u->blocks[b - 1].kind;
        u32 node     = u->blocks[b - 1].node;
        StrObj *name = u->blocks[b - 1].name;
        bool busy    = u->blocks[b - 1].busy;

        u32 pops = u->blocks[b - 1].pops;

        switch (kind) {
        case FK::Loop:
            // A `return` leaves the loop's iterator behind, and anything that
            // runs on the way out -- a with, a finally -- reads the stack at a
            // fixed depth, so it has to go.
            for (u32 k = 0; k < pops; k++) {
                if (preserve_tos && !emit(Bc::RotTwo, node))
                    return false;
                if (!emit(Bc::PopTop, node))
                    return false;
            }
            break;

        case FK::Try:
            if (!emit(Bc::PopBlock, node))
                return false;
            break;

        case FK::Finally: {
            // Already being emitted: this exit is *inside* the clause, whose
            // handler has been popped and whose body is running, so there is
            // nothing left here to unwind.
            if (busy)
                break;
            if (!emit(Bc::PopBlock, node))
                return false;
            // A `return` keeps its value on the stack while the clause runs,
            // so a `break` out of the clause has to drop it.
            const Node &n         = ast->at(node);
            u->blocks[b - 1].busy = true;
            u->pending += preserve_tos ? 1 : 0;
            bool ok = stmts(node, n.a + n.b + n.c, n.d);
            u->pending -= preserve_tos ? 1 : 0;
            u->blocks[b - 1].busy = false;
            if (!ok)
                return false;
            break;
        }

        case FK::With:
            if (preserve_tos && !emit(Bc::RotTwo, node))
                return false;
            if (!emit(Bc::PopBlock, node) || !emit(Bc::LoadConst, const_none(), node) ||
                !emit(Bc::DupTop, node) || !emit(Bc::DupTop, node) || !emit(Bc::Call, 3, node))
                return false;
            if (u->blocks[b - 1].async && !await_top(node, AW_AEXIT))
                return false;
            if (!emit(Bc::PopTop, node))
                return false;
            break;

        case FK::StarBody:
            return fail("'break', 'continue' and 'return' cannot appear in an except* block",
                        u->blocks[b - 1].node);

        case FK::Handler:
            if (preserve_tos && !emit(Bc::RotTwo, node))
                return false;
            if (!emit(Bc::PopBlock, node))
                return false;
            if (name) {
                if (!emit(Bc::LoadConst, const_none(), node) || !store_name(name, node) ||
                    !del_name(name, node))
                    return false;
            }
            if (!emit(Bc::PopExcept, node))
                return false;
            break;
        }
    }
    return true;
}

bool Compiler::loop_exit(bool is_break, u32 node)
{
    usize b = u->blocks.size();
    while (b && u->blocks[b - 1].kind != FK::Loop)
        b--;
    if (!b)
        return fail(is_break ? "'break' outside loop" : "'continue' not properly in loop", node);

    if (!unwind(b, false))
        return false;
    // Only what an enclosing finally clause left below this loop: a loop
    // opened inside the clause is left without touching what the clause
    // holds, so `break` there does not swallow the exception being unwound.
    for (u32 k = u->blocks[b - 1].held; k < u->pending; k++)
        if (!emit(Bc::PopTop, node))
            return false;
    if (is_break) {
        for (u32 k = 0; k < u->blocks[b - 1].pops; k++)
            if (!emit(Bc::PopTop, node))
                return false;
        u32 j = emit_jump(Bc::Jump, node);
        return u->blocks[b - 1].breaks.push(j) ? true : oom();
    }
    return emit(Bc::Jump, u->blocks[b - 1].cont, node);
}

// ------------------------------------------------------------- expressions

bool Compiler::sequence(u32 i, Nd kind)
{
    const Node &n = ast->at(i);
    bool star     = has_star(i, 0, n.nkid);

    if (!star) {
        if (!exprs(i, 0, n.nkid))
            return false;
        Bc op = kind == Nd::List ? Bc::BuildList : kind == Nd::Set ? Bc::BuildSet : Bc::BuildTuple;
        return emit(op, n.nkid, i);
    }

    bool set = kind == Nd::Set;
    if (!emit(set ? Bc::BuildSet : Bc::BuildList, 0, i))
        return false;
    for (u32 k = 0; k < n.nkid; k++) {
        u32 e         = kid(i, k);
        const Node &x = ast->at(e);
        if (x.kind == Nd::Starred) {
            if (!expr(x.a) || !emit(set ? Bc::SetUpdate : Bc::ListExtend, 1, e))
                return false;
        } else if (!expr(e) || !emit(set ? Bc::SetAdd : Bc::ListAppend, 1, e)) {
            return false;
        }
    }
    return kind == Nd::Tuple ? emit(Bc::ListToTuple, i) : true;
}

bool Compiler::dict(u32 i)
{
    const Node &n = ast->at(i);
    u32 pairs     = n.nkid / 2;
    bool splat    = false;
    for (u32 k = 0; k < pairs; k++)
        splat = splat || kid(i, k * 2) == 0;

    if (!splat) {
        for (u32 k = 0; k < pairs; k++)
            if (!expr(kid(i, k * 2)) || !expr(kid(i, k * 2 + 1)))
                return false;
        return emit(Bc::BuildMap, pairs, i);
    }

    if (!emit(Bc::BuildMap, 0, i))
        return false;
    for (u32 k = 0; k < pairs; k++) {
        u32 key = kid(i, k * 2), val = kid(i, k * 2 + 1);
        if (!key) {
            if (!expr(val) || !emit(Bc::DictUpdate, 1, val))
                return false;
        } else if (!expr(key) || !expr(val) || !emit(Bc::MapAdd, 1, val)) {
            return false;
        }
    }
    return true;
}

bool Compiler::boolop(u32 i)
{
    const Node &n = ast->at(i);
    Bc op         = Bool(n.flags) == Bool::And ? Bc::JumpIfFalseOrPop : Bc::JumpIfTrueOrPop;
    Vec<u32> ends;
    for (u32 k = 0; k < n.nkid; k++) {
        if (!expr(kid(i, k)))
            return false;
        if (k + 1 < n.nkid && !ends.push(emit_jump(op, i)))
            return oom();
    }
    patch_all(ends);
    return !failed;
}

bool Compiler::compare(u32 i)
{
    const Node &n = ast->at(i);
    if (!expr(n.a))
        return false;
    if (n.nkid == 1) {
        const Node &c = ast->at(kid(i, 0));
        return expr(c.a) && emit(Bc::CompareOp, c.flags, kid(i, 0));
    }

    // A chain keeps the last operand for the next rung, so each but the last
    // is duplicated and slid under the result.
    Vec<u32> fails;
    for (u32 k = 0; k < n.nkid; k++) {
        u32 c         = kid(i, k);
        const Node &x = ast->at(c);
        if (!expr(x.a))
            return false;
        if (k + 1 < n.nkid && (!emit(Bc::DupTop, c) || !emit(Bc::RotThree, c)))
            return false;
        if (!emit(Bc::CompareOp, x.flags, c))
            return false;
        if (k + 1 < n.nkid && !fails.push(emit_jump(Bc::JumpIfFalseOrPop, c)))
            return oom();
    }
    u32 done = emit_jump(Bc::Jump, i);
    patch_all(fails);
    if (!emit(Bc::RotTwo, i) || !emit(Bc::PopTop, i))
        return false;
    patch(done);
    return !failed;
}

// The arguments and the call opcode. `pre` counts values already pushed above
// the callable, which a class definition uses for the body and the name.
// `extra`, when given, is one more positional argument after the rest: a
// generic class's base.
bool Compiler::call_args(u32 i, u32 at, u32 nargs, u32 nkw, u32 pre, u32 node, StrObj *extra)
{
    bool star  = has_star(i, at, nargs);
    bool dstar = false;
    for (u32 k = 0; k < nkw; k++)
        dstar = dstar || ast->at(kid(i, at + nargs + k)).flags == 0;

    u32 more = extra ? 1 : 0;
    if (!star && !dstar) {
        if (!exprs(i, at, nargs) || (extra && !load_name(extra, node)))
            return false;
        nargs += more;
        if (!nkw)
            return emit(Bc::Call, pre + nargs, node);

        Root names{ obj_value(tuple_new(nkw)) };
        if (names.v.is_nil())
            return oom();
        for (u32 k = 0; k < nkw; k++) {
            u32 w = kid(i, at + nargs - more + k);
            if (!expr(ast->at(w).a))
                return false;
            StrObj *s = raw_ident(w);
            if (!s)
                return oom();
            static_cast<TupleObj *>(names.v.obj())->items()[k] = obj_value(s);
        }
        return emit(Bc::LoadConst, add_const(names.v), node) &&
               emit(Bc::CallKw, pre + nargs + nkw, node);
    }

    // A splat: the positional arguments become a tuple and the keywords a
    // mapping, so the call takes two values whatever the shape.
    if (!emit(Bc::BuildList, pre, node))
        return false;
    for (u32 k = 0; k < nargs; k++) {
        u32 e         = kid(i, at + k);
        const Node &x = ast->at(e);
        if (x.kind == Nd::Starred) {
            if (!expr(x.a) || !emit(Bc::ListExtend, 1, e))
                return false;
        } else if (!expr(e) || !emit(Bc::ListAppend, 1, e)) {
            return false;
        }
    }
    if (extra && (!load_name(extra, node) || !emit(Bc::ListAppend, 1, node)))
        return false;
    if (!emit(Bc::ListToTuple, node))
        return false;
    if (!nkw)
        return emit(Bc::CallEx, 0, node);

    if (!emit(Bc::BuildMap, 0, node))
        return false;
    // Every keyword goes in through DictMerge, a named one as a one-entry map,
    // so that a duplicate is refused however it arrives. CPython's shape.
    for (u32 k = 0; k < nkw; k++) {
        u32 w         = kid(i, at + nargs + k);
        const Node &x = ast->at(w);
        if (x.flags) {
            StrObj *s = raw_ident(w);
            if (!s)
                return oom();
            if (!emit(Bc::LoadConst, add_const(obj_value(s)), w) || !expr(x.a) ||
                !emit(Bc::BuildMap, 1, w))
                return false;
        } else if (!expr(x.a)) {
            return false;
        }
        if (!emit(Bc::DictMerge, 1, w))
            return false;
    }
    return emit(Bc::CallEx, CX_KWARGS, node);
}

bool Compiler::call(u32 i)
{
    const Node &n = ast->at(i);
    return expr(n.a) && call_args(i, 0, n.b, n.nkid - n.b, 0, i);
}

// The closure tuple a nested scope needs, and the MakeFunction bit for it.
bool Compiler::closure_of(u32 node, u32 &flags)
{
    return closure_of_scope(st.at_node[node], node, flags);
}

bool Compiler::closure_of_scope(u32 scope, u32 node, u32 &flags)
{
    const Scope &s = st.scopes[scope];
    if (!s.freevars.size())
        return true;
    for (usize k = 0; k < s.freevars.size(); k++)
        if (!load_cell(s.syms[s.freevars[k]].name, node))
            return false;
    flags |= MF_CLOSURE;
    return emit(Bc::BuildTuple, u32(s.freevars.size()), node);
}

bool Compiler::function(u32 i, bool as_statement)
{
    const Node &n = ast->at(i);
    bool lambda   = n.kind == Nd::Lambda;
    u32 ndecor    = lambda ? 0 : n.c;

    for (u32 k = 0; k < ndecor; k++)
        if (!expr(kid(i, n.b + k)))
            return false;

    u32 flags = 0;
    if (n.a) {
        const Node &a = ast->at(n.a);
        u32 vararg    = (a.flags & ARG_VARARG) ? 1 : 0;
        u32 kwarg     = (a.flags & ARG_KWARG) ? 1 : 0;
        u32 at_kwdef  = a.a + a.b + vararg + a.c;
        u32 at_def    = at_kwdef + a.c + kwarg;

        if (a.d) {
            if (!exprs(n.a, at_def, a.d) || !emit(Bc::BuildTuple, a.d, i))
                return false;
            flags |= MF_DEFAULTS;
        }
        u32 nkwdef = 0;
        for (u32 k = 0; k < a.c; k++)
            if (kid(n.a, at_kwdef + k))
                nkwdef++;
        if (nkwdef) {
            for (u32 k = 0; k < a.c; k++) {
                u32 d = kid(n.a, at_kwdef + k);
                if (!d)
                    continue;
                StrObj *s = ident(kid(n.a, a.a + a.b + vararg + k));
                if (!s)
                    return oom();
                if (!emit(Bc::LoadConst, add_const(obj_value(s)), d) || !expr(d))
                    return false;
            }
            if (!emit(Bc::BuildMap, nkwdef, i))
                return false;
            flags |= MF_KWDEFAULTS;
        }
    }

    if (!lambda && n.pad) {
        if (!generic_function(i, flags))
            return false;
    } else {
        if (!closure_of(i, flags) || (!lambda && !annotate_of(i, flags)))
            return false;
        u32 k = nested(i);
        if (failed)
            return false;
        if (!emit(Bc::LoadConst, k, i) || !emit(Bc::MakeFunction, flags, i))
            return false;
    }
    for (u32 d = 0; d < ndecor; d++)
        if (!emit(Bc::Call, 1, i))
            return false;
    if (!as_statement)
        return true;
    StrObj *nm = ident(i);
    return nm ? store_name(nm, i) : oom();
}

bool Compiler::classdef(u32 i)
{
    const Node &n = ast->at(i);
    for (u32 k = 0; k < n.d; k++)
        if (!expr(kid(i, n.a + n.b + n.c + k)))
            return false;

    if (n.pad) {
        if (!generic_class(i))
            return false;
        for (u32 d = 0; d < n.d; d++)
            if (!emit(Bc::Call, 1, i))
                return false;
        StrObj *nm = ident(i);
        return nm ? store_name(nm, i) : oom();
    }

    if (!emit(Bc::LoadBuildClass, i))
        return false;
    u32 flags = 0;
    if (!closure_of(i, flags))
        return false;
    u32 k = nested(i);
    if (failed)
        return false;
    StrObj *nm = raw_ident(i);
    if (!nm)
        return oom();
    if (!emit(Bc::LoadConst, k, i) || !emit(Bc::MakeFunction, flags, i) ||
        !emit(Bc::LoadConst, add_const(obj_value(nm)), i))
        return false;
    if (!call_args(i, 0, n.a, n.b, 2, i))
        return false;
    for (u32 d = 0; d < n.d; d++)
        if (!emit(Bc::Call, 1, i))
            return false;
    nm = ident(i);
    return nm ? store_name(nm, i) : oom();
}

bool Compiler::comprehension(u32 i)
{
    u32 flags = 0;
    if (!closure_of(i, flags))
        return false;
    u32 k = nested(i);
    if (failed)
        return false;
    // The outermost iterable is evaluated here and passed in as the argument.
    const Node &n  = ast->at(i);
    bool aiter     = (ast->at(kid(i, 0)).flags & 1) != 0;
    const Scope &s = st.scopes[st.at_node[i]];
    if (!emit(Bc::LoadConst, k, i) || !emit(Bc::MakeFunction, flags, i) ||
        !expr(ast->at(kid(i, 0)).b) || !emit(aiter ? Bc::GetAIter : Bc::GetIter, i) ||
        !emit(Bc::Call, 1, i))
        return false;
    // A comprehension that awaits is a coroutine, and is awaited here. An
    // async generator expression is not: it is what the expression is worth.
    if (s.coroutine && n.kind != Nd::GeneratorExp)
        return await_top(i, AW_AWAIT);
    return true;
}

// A Constant node's value. Nil with the error pending.
Value Compiler::const_of(u32 i)
{
    const Node &n  = ast->at(i);
    const Token &t = ast->lex.tokens[n.tok];
    Value v;
    switch (Const(n.flags)) {
    case Const::None:
        v = value_none();
        break;
    case Const::True:
        v = value_bool(true);
        break;
    case Const::False:
        v = value_bool(false);
        break;
    case Const::Ellipsis:
        v = value_ellipsis();
        break;
    case Const::Int:
        v = (t.flags & TOK_INT_WIDE) ? int_parse(ast->lex.text_of(t), tok_int_base(t.flags))
                                     : int_from_i64(t.ival);
        break;
    case Const::Float:
        v = float_new(t.fval);
        break;
    case Const::Imag:
        v = complex_new(0, t.fval);
        break;
    case Const::Str:
        v = obj_value(str_raw(ast->lex.text_of(t)));
        break;
    case Const::Bytes:
        v = bytes_new(ast->lex.text_of(t));
        break;
    }
    return v;
}

// A literal pattern's value, folded the way CPython's optimizer folds `-1`
// and `1+2j` before code is made. Nil for anything else, with no error.
Value Compiler::literal_of(u32 i)
{
    const Node &n = ast->at(i);
    if (n.kind == Nd::Constant)
        return const_of(i);
    if (n.kind == Nd::UnaryOp && Un(n.flags) == Un::USub) {
        Root v{ literal_of(n.a) };
        Value out;
        if (v.v.is_nil() || py_neg(v.v, out) != R::Ok)
            return Value();
        return out;
    }
    if (n.kind == Nd::BinOp) {
        Root l{ literal_of(n.a) };
        Root r{ literal_of(n.b) };
        Value out;
        if (l.v.is_nil() || r.v.is_nil() || py_binop(l.v, r.v, Op(n.flags), out) != R::Ok)
            return Value();
        return out;
    }
    return Value();
}

bool Compiler::expr(u32 i)
{
    if (failed)
        return false;
    if (!i)
        return true;
    const Node &n = ast->at(i);

    switch (n.kind) {
    case Nd::Constant: {
        Value v = const_of(i);
        if (v.is_nil())
            return failed = true, false;
        return emit(Bc::LoadConst, add_const(v), i);
    }

    case Nd::Name: {
        StrObj *s = ident(i);
        if (s && s->str() == "__debug__")
            return emit(Bc::LoadConst, add_const(value_bool(level() == 0)), i);
        return s ? load_name(s, i) : oom();
    }

    case Nd::Tuple:
    case Nd::List:
    case Nd::Set:
        return sequence(i, n.kind);
    case Nd::Dict:
        return dict(i);

    case Nd::BinOp:
        return expr(n.a) && expr(n.b) && emit(Bc::BinaryOp, n.flags, i);
    case Nd::UnaryOp:
        return expr(n.a) && emit(Bc::UnaryOp, n.flags, i);
    case Nd::BoolOp:
        return boolop(i);
    case Nd::Compare:
        return compare(i);

    case Nd::IfExp: {
        if (!expr(n.a))
            return false;
        u32 other = emit_jump(Bc::PopJumpIfFalse, i);
        if (!expr(n.b))
            return false;
        u32 end = emit_jump(Bc::Jump, i);
        patch(other);
        if (!expr(n.c))
            return false;
        patch(end);
        return !failed;
    }

    case Nd::NamedExpr:
        return expr(n.b) && emit(Bc::DupTop, i) && store(n.a);

    case Nd::Call:
        return call(i);

    case Nd::Attribute: {
        StrObj *s = ident(i);
        return s && expr(n.a) && emit(Bc::LoadAttr, name_index(s), i);
    }

    case Nd::Subscript:
        return expr(n.a) && expr(n.b) && emit(Bc::LoadSubscr, i);

    case Nd::Slice: {
        if (!(n.a ? expr(n.a) : emit(Bc::LoadConst, const_none(), i)))
            return false;
        if (!(n.b ? expr(n.b) : emit(Bc::LoadConst, const_none(), i)))
            return false;
        if (!n.c)
            return emit(Bc::BuildSlice, 2, i);
        return expr(n.c) && emit(Bc::BuildSlice, 3, i);
    }

    case Nd::Lambda:
        return function(i, false);

    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::DictComp:
    case Nd::GeneratorExp:
        return comprehension(i);

    case Nd::Yield:
        if (!(n.a ? expr(n.a) : emit(Bc::LoadConst, const_none(), i)))
            return false;
        if (async_gen() && !emit(Bc::AsyncGenWrap, i))
            return false;
        return emit(Bc::YieldValue, i);
    case Nd::YieldFrom:
        // YieldFrom takes the iterator and the value sent to it. It comes
        // back to itself with the next value sent in.
        return expr(n.a) && emit(Bc::GetYieldFromIter, i) && emit(Bc::LoadConst, const_none(), i) &&
               emit(Bc::YieldFrom, i);

    case Nd::Starred:
        return fail("can't use starred expression here", i);
    case Nd::JoinedStr: {
        // Each piece becomes a string on the stack and BuildString joins them.
        // An empty f-string is the empty constant, and one piece that is
        // already a constant needs no join at all.
        if (!n.nkid)
            return emit(Bc::LoadConst, add_const(str_new(Str(""))), i);
        // One piece is already a str -- a constant, or what FormatValue
        // leaves -- so there is nothing to join.
        if (n.nkid == 1)
            return expr(ast->kids[n.kid0]);
        for (u32 k = 0; k < n.nkid; k++)
            if (!expr(ast->kids[n.kid0 + k]))
                return false;
        return emit(Bc::BuildString, n.nkid, i);
    }

    case Nd::FormattedValue: {
        if (!expr(n.a))
            return false;
        u32 flags = n.flags;
        if (n.b) {
            if (!expr(n.b))
                return false;
            flags |= FV_SPEC;
        }
        return emit(Bc::FormatValue, flags, i);
    }
    case Nd::TemplateStr: {
        // A tuple of the strings, an empty one wherever two interpolations
        // meet or one is at either end, then a tuple of the interpolations.
        bool gap = true;
        u32 ns = 0, ni = 0;
        for (u32 k = 0; k < n.nkid; k++) {
            u32 part = ast->kids[n.kid0 + k];
            if (ast->at(part).kind == Nd::Interpolation) {
                if (gap && !empty_str(ns, i))
                    return false;
                gap = true;
                ni++;
            } else {
                if (!expr(part))
                    return false;
                ns++;
                gap = false;
            }
        }
        if (gap && !empty_str(ns, i))
            return false;
        if (!emit(Bc::BuildTuple, ns, i))
            return false;
        for (u32 k = 0; k < n.nkid; k++) {
            u32 part = ast->kids[n.kid0 + k];
            if (ast->at(part).kind == Nd::Interpolation && !expr(part))
                return false;
        }
        return emit(Bc::BuildTuple, ni, i) && emit(Bc::BuildTemplate, i);
    }

    case Nd::Interpolation: {
        if (!expr(n.a) || !expr(n.c))
            return false;
        u32 flags = n.flags;
        if (n.b) {
            if (!expr(n.b))
                return false;
            flags |= FV_SPEC;
        }
        return emit(Bc::BuildInterpolation, flags, i);
    }

    case Nd::Await:
        return expr(n.a) && await_top(i, AW_AWAIT);

    default:
        return fail("this expression is not compiled yet", i);
    }
}

// -------------------------------------------------------------- assignment

bool Compiler::store(u32 i)
{
    if (failed)
        return false;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Name: {
        StrObj *s = ident(i);
        return s ? store_name(s, i) : oom();
    }
    case Nd::Attribute: {
        StrObj *s = ident(i);
        return s && note_static(i) && expr(n.a) && emit(Bc::StoreAttr, name_index(s), i);
    }
    case Nd::Subscript:
        return expr(n.a) && expr(n.b) && emit(Bc::StoreSubscr, i);

    case Nd::Tuple:
    case Nd::List: {
        u32 star = n.nkid;
        for (u32 k = 0; k < n.nkid; k++)
            if (ast->at(kid(i, k)).kind == Nd::Starred) {
                if (star != n.nkid)
                    return fail("two starred expressions in assignment", i);
                star = k;
            }
        if (star == n.nkid) {
            if (!emit(Bc::UnpackSequence, n.nkid, i))
                return false;
        } else if (!emit(Bc::UnpackEx, star | ((n.nkid - 1 - star) << 16), i)) {
            return false;
        }
        for (u32 k = 0; k < n.nkid; k++) {
            u32 t = kid(i, k);
            if (!store(ast->at(t).kind == Nd::Starred ? ast->at(t).a : t))
                return false;
        }
        return true;
    }

    case Nd::Starred:
        return fail("starred assignment target must be in a list or tuple", i);
    default:
        return fail("cannot assign to this", i);
    }
}

bool Compiler::del(u32 i)
{
    if (failed)
        return false;
    const Node &n = ast->at(i);
    switch (n.kind) {
    case Nd::Name: {
        StrObj *s = ident(i);
        return s ? del_name(s, i) : oom();
    }
    case Nd::Attribute: {
        StrObj *s = ident(i);
        return s && expr(n.a) && emit(Bc::DeleteAttr, name_index(s), i);
    }
    case Nd::Subscript:
        return expr(n.a) && expr(n.b) && emit(Bc::DeleteSubscr, i);
    case Nd::Tuple:
    case Nd::List:
        for (u32 k = 0; k < n.nkid; k++)
            if (!del(kid(i, k)))
                return false;
        return true;
    default:
        return fail("cannot delete this", i);
    }
}

// ------------------------------------------------------------------ import

// A dotted module name, as the span of tokens it was written as.
bool dotted_name(const Ast &ast, u32 from, u32 to, String &out)
{
    for (u32 k = from; k < to; k++) {
        const Token &t = ast.lex.tokens[k];
        if (!(t.kind == Tok::Name ? out.append(ast.lex.text_of(t)) : out.push('.')))
            return false;
    }
    return true;
}

// The import opcode for statement `n`: lazy as written, or as the run time
// decides, or never -- inside a try, and for `import *`.
Bc Compiler::import_op(u32 n, bool star)
{
    if (ast->at(n).pad & 1)
        return Bc::LazyImportName;
    if (star)
        return Bc::ImportNameEager;
    for (usize b = 0; b < u->blocks.size(); b++) {
        FK k = u->blocks[b].kind;
        if (k == FK::Try || k == FK::Finally || k == FK::Handler || k == FK::StarBody)
            return Bc::ImportNameEager;
    }
    return Bc::ImportName;
}

bool Compiler::import(u32 i)
{
    const Node &n = ast->at(i);
    bool lazy     = (n.pad & 1) != 0;
    for (u32 k = 0; k < n.nkid; k++) {
        u32 a         = kid(i, k);
        const Node &x = ast->at(a);
        String dotted;
        if (!dotted_name(*ast, x.tok, x.b ? x.b : x.tok + 1, dotted))
            return oom();
        StrObj *full = mangled(dotted.str());
        if (!full)
            return oom();
        if (!emit(Bc::LoadConst, add_const(Value::of_int(0)), a) ||
            !emit(Bc::LoadConst, const_none(), a) ||
            !emit(import_op(i, false), name_index(full), a))
            return false;

        // Without an `as` the top package is bound; with one, the submodule is
        // walked to and that is what the name gets. A lazy one walks by
        // ImportFrom, which a lazy import answers lazily.
        StrObj *bound = nullptr;
        if (x.a) {
            for (u32 t = x.tok + 1; t + 1 < (x.b ? x.b : x.tok + 1); t += 2) {
                StrObj *part = mangled(ast->lex.text_of(ast->lex.tokens[t + 1]));
                if (!part)
                    return oom();
                if (lazy) {
                    if (!emit(Bc::ImportFrom, name_index(part), a) || !emit(Bc::RotTwo, a) ||
                        !emit(Bc::PopTop, a))
                        return false;
                } else if (!emit(Bc::LoadAttr, name_index(part), a)) {
                    return false;
                }
            }
            bound = mangled(ast->lex.text_of(ast->lex.tokens[x.a - 1]));
        } else {
            bound = mangled(ast->lex.text_of(ast->lex.tokens[x.tok]));
        }
        if (!bound || !store_name(bound, a))
            return false;
    }
    return true;
}

bool Compiler::import_from(u32 i)
{
    const Node &n = ast->at(i);
    String dotted;
    if (n.flags && !dotted_name(*ast, n.tok, n.b, dotted))
        return oom();
    StrObj *module = mangled(dotted.str());
    if (!module)
        return oom();

    bool star = n.nkid == 1 && (ast->at(kid(i, 0)).flags & 1);
    Root names{ obj_value(tuple_new(n.nkid)) };
    if (names.v.is_nil())
        return oom();
    for (u32 k = 0; k < n.nkid; k++) {
        u32 a     = kid(i, k);
        StrObj *s = str_intern(star ? Str("*") : ast->lex.text_of(ast->lex.tokens[ast->at(a).tok]));
        if (!s)
            return oom();
        static_cast<TupleObj *>(names.v.obj())->items()[k] = obj_value(s);
    }

    if (!emit(Bc::LoadConst, add_const(Value::of_int(i32(n.a))), i) ||
        !emit(Bc::LoadConst, add_const(names.v), i) ||
        !emit(import_op(i, star), name_index(module), i))
        return false;
    if (star)
        return emit(Bc::ImportStar, i);

    for (u32 k = 0; k < n.nkid; k++) {
        u32 a         = kid(i, k);
        const Node &x = ast->at(a);
        StrObj *from  = mangled(ast->lex.text_of(ast->lex.tokens[x.tok]));
        StrObj *bound = x.a ? mangled(ast->lex.text_of(ast->lex.tokens[x.a - 1])) : from;
        if (!from || !bound)
            return oom();
        if (!emit(Bc::ImportFrom, name_index(from), a) || !store_name(bound, a))
            return false;
    }
    return emit(Bc::PopTop, i);
}

// ---------------------------------------------------------------- PEP 695
//
// CPython's shape: a generic definition is made inside a function of its own,
// the scope of its type parameters, which is called at once. A bound, a
// default and an alias's value are each a function too, called when first
// asked for.

// The type parameters of `n`, from its run at `from`, each made and bound to
// its name; a tuple of them is left on the stack.
bool Compiler::emit_type_params(u32 n, u32 from, u32 count)
{
    bool seen_default = false;
    for (u32 k = 0; k < count; k++) {
        u32 tp        = kid(n, from + k);
        const Node &p = ast->at(tp);
        StrObj *raw   = raw_ident(tp);
        StrObj *name  = ident(tp);
        if (!raw || !name)
            return oom();
        if (!emit(Bc::LoadConst, add_const(obj_value(raw)), tp))
            return false;
        u32 kind = p.kind == Nd::ParamSpec      ? TI_PARAMSPEC
                   : p.kind == Nd::TypeVarTuple ? TI_TYPEVARTUPLE
                                                : TI_TYPEVAR;
        if (p.kind == Nd::TypeVar && p.a) {
            if (!anno_function(tp, AN_BOUND, raw))
                return false;
            kind = ast->at(p.a).kind == Nd::Tuple ? TI_TYPEVAR_CONSTRAINTS : TI_TYPEVAR_BOUND;
        }
        if (!emit(Bc::Intrinsic, kind, tp))
            return false;
        if (p.b) {
            seen_default = true;
            if (!anno_function(tp, AN_DEFAULT, raw) || !emit(Bc::Intrinsic, TI_SET_DEFAULT, tp))
                return false;
        } else if (seen_default) {
            Buf<128> b;
            b.put("non-default type parameter '").put(raw->str());
            b.put("' follows default type parameter");
            return fail(b.str(), tp);
        }
        if (!emit(Bc::Copy, 1, tp) || !store_name(name, tp))
            return false;
    }
    return emit(Bc::BuildTuple, count, n);
}

// A bound, a default or a value, as the function that evaluates it.
bool Compiler::anno_function(u32 node, u8 role, StrObj *name)
{
    u32 scope = st.anno(node, role);
    if (!scope)
        return fail("an annotation scope is missing", node);
    u32 flags = 0;
    if (!closure_of_scope(scope, node, flags))
        return false;
    u32 k = nested_scope(scope, node, name, role);
    if (failed)
        return false;
    return emit(Bc::LoadConst, k, node) && emit(Bc::MakeFunction, flags, node);
}

// What an annotation scope's code does.
bool Compiler::anno_body(u32 node, u8 role)
{
    const Node &n = ast->at(node);
    if (role != AN_PARAMS && !annotate_prologue(node))
        return false;
    switch (role) {
    case AN_BOUND:
        return expr(n.a) && emit(Bc::Return, node);
    case AN_DEFAULT:
        if (n.kind == Nd::TypeVarTuple && ast->at(n.b).kind == Nd::Starred)
            return expr(ast->at(n.b).a) && emit(Bc::UnpackSequence, 1, node) &&
                   emit(Bc::Return, node);
        return expr(n.b) && emit(Bc::Return, node);
    case AN_VALUE:
        return expr(n.b) && emit(Bc::Return, node);
    case AN_ANNOTATE:
        if (n.kind == Nd::Module || n.kind == Nd::ClassDef)
            return annotate_deferred(node);
        return annotate_args(node);
    default:
        break;
    }

    // AN_PARAMS: the type parameters, then what they parameterize.
    if (n.kind == Nd::FunctionDef || n.kind == Nd::AsyncFunctionDef) {
        if (!emit_type_params(node, n.b + n.c, n.pad))
            return false;
        u32 nargs = co()->argcount;
        for (u32 k = 0; k < nargs; k++)
            if (!emit(Bc::LoadFast, k, node))
                return false;
        u32 flags     = 0;
        const Node &a = ast->at(n.a);
        u32 vararg    = (a.flags & ARG_VARARG) ? 1 : 0;
        u32 kwarg     = (a.flags & ARG_KWARG) ? 1 : 0;
        u32 at_kwdef  = a.a + a.b + vararg + a.c;
        bool kwdef    = false;
        for (u32 k = 0; k < a.c; k++)
            kwdef = kwdef || kid(n.a, at_kwdef + k) != 0;
        (void)kwarg;
        flags |= a.d ? MF_DEFAULTS : 0;
        flags |= kwdef ? MF_KWDEFAULTS : 0;
        if (!closure_of(node, flags) || !annotate_of(node, flags))
            return false;
        u32 k = nested(node);
        if (failed)
            return false;
        return emit(Bc::LoadConst, k, node) && emit(Bc::MakeFunction, flags, node) &&
               emit(Bc::RotTwo, node) && emit(Bc::Intrinsic, TI_FUNCTION_TYPE_PARAMS, node) &&
               emit(Bc::Return, node);
    }
    if (n.kind == Nd::ClassDef) {
        StrObj *tps  = str_intern(".type_params");
        StrObj *base = str_intern(".generic_base");
        StrObj *nm   = raw_ident(node);
        if (!tps || !base || !nm)
            return oom();
        if (!emit_type_params(node, n.a + n.b + n.c + n.d, n.pad) || !store_name(tps, node) ||
            !emit(Bc::LoadBuildClass, node))
            return false;
        u32 flags = 0;
        if (!closure_of(node, flags))
            return false;
        u32 k = nested(node);
        if (failed)
            return false;
        if (!emit(Bc::LoadConst, k, node) || !emit(Bc::MakeFunction, flags, node) ||
            !emit(Bc::LoadConst, add_const(obj_value(nm)), node) || !load_name(tps, node) ||
            !emit(Bc::Intrinsic, TI_SUBSCRIPT_GENERIC, node) || !store_name(base, node))
            return false;
        return call_args(node, 0, n.a, n.b, 2, node, base) && emit(Bc::Return, node);
    }
    // A type alias: its name, its parameters and its value.
    StrObj *nm = raw_ident(n.a);
    if (!nm)
        return oom();
    return emit(Bc::LoadConst, add_const(obj_value(nm)), node) && emit_type_params(node, 0, n.c) &&
           anno_function(node, AN_VALUE, nm) && emit(Bc::BuildTuple, 3, node) &&
           emit(Bc::Intrinsic, TI_TYPEALIAS, node) && emit(Bc::Return, node);
}

// ---------------------------------------------------------------- PEP 649
//
// An annotation is not evaluated where it is written. The compiler collects
// the annotations of a def, a class body or a module into __annotate__, a
// function of one argument.

// The __annotate__ of `node`, left under the code object. Nothing is emitted
// where there are no annotations.
bool Compiler::annotate_of(u32 node, u32 &flags)
{
    u32 scope = st.anno(node, AN_ANNOTATE);
    if (!scope)
        return true;
    StrObj *name = str_intern("__annotate__");
    if (!name)
        return oom();
    u32 sub = 0;
    if (!closure_of_scope(scope, node, sub))
        return false;
    u32 k = nested_scope(scope, node, name, AN_ANNOTATE);
    if (failed)
        return false;
    if (!emit(Bc::LoadConst, k, node) || !emit(Bc::MakeFunction, sub, node))
        return false;
    flags |= MF_ANNOTATE;
    return true;
}

// `if .format > VALUE_WITH_FAKE_GLOBALS: raise NotImplementedError`, which is
// how annotationlib knows to evaluate the annotations itself.
bool Compiler::annotate_prologue(u32 node)
{
    const ExcType *nie = exc_find("NotImplementedError");
    Value cls          = nie ? exc_type_value(nie) : Value();
    if (cls.is_nil())
        return oom();
    if (!emit(Bc::LoadFast, 0, node) ||
        !emit(Bc::LoadConst, add_const(Value::of_int(ANN_FAKEGLOBAL)), node) ||
        !emit(Bc::CompareOp, u32(Cmp::Gt), node))
        return false;
    u32 skip = emit_jump(Bc::PopJumpIfFalse, node);
    if (!emit(Bc::LoadConst, add_const(cls), node) || !emit(Bc::Raise, 1, node))
        return false;
    patch(skip);
    return true;
}

// The dict a class's or a module's __annotate__ answers: one entry per
// deferred `x: T`, less those whose statement did not run.
bool Compiler::annotate_deferred(u32 node)
{
    u32 owner     = st.at_node[node];
    StrObj *conds = str_intern("__conditional_annotations__");
    if (!conds)
        return oom();
    if (!emit(Bc::BuildMap, 0, node))
        return false;
    for (usize k = 0; k < st.scopes[owner].deferred.size(); k++) {
        u32 a    = st.scopes[owner].deferred[k];
        i32 idx  = st.scopes[owner].cond[k];
        u32 skip = 0;
        if (idx >= 0) {
            if (!emit(Bc::LoadConst, add_const(Value::of_int(idx)), a) || !load_name(conds, a) ||
                !emit(Bc::CompareOp, u32(Cmp::In), a))
                return false;
            skip = emit_jump(Bc::PopJumpIfFalse, a);
        }
        StrObj *nm = ident(ast->at(a).a);
        if (!nm)
            return oom();
        if (!expr(ast->at(a).b) || !emit(Bc::Copy, 2, a) ||
            !emit(Bc::LoadConst, add_const(obj_value(nm)), a) || !emit(Bc::StoreSubscr, a))
            return false;
        if (idx >= 0)
            patch(skip);
    }
    return emit(Bc::Return, node);
}

// One parameter's name and annotation, as a pair for the dict below.
bool Compiler::annotate_one(u32 arg, u32 &count)
{
    if (!arg || !ast->at(arg).a)
        return true;
    u32 ann    = ast->at(arg).a;
    StrObj *nm = ident(arg);
    if (!nm)
        return oom();
    if (!emit(Bc::LoadConst, add_const(obj_value(nm)), arg))
        return false;
    count++;
    // `*args: *Ts`, whose annotation is the unpacking rather than the tuple.
    if (ast->at(ann).kind == Nd::Starred)
        return expr(ast->at(ann).a) && emit(Bc::UnpackSequence, 1, ann);
    return expr(ann);
}

// The dict a def's __annotate__ answers: the parameters, then the return.
bool Compiler::annotate_args(u32 node)
{
    const Node &n = ast->at(node);
    u32 count     = 0;
    if (n.a) {
        const Node &a = ast->at(n.a);
        u32 vararg    = (a.flags & ARG_VARARG) ? 1 : 0;
        u32 kwarg     = (a.flags & ARG_KWARG) ? 1 : 0;
        u32 at_kwdef  = a.a + a.b + vararg + a.c;
        for (u32 k = 0; k < at_kwdef; k++)
            if (!annotate_one(kid(n.a, k), count))
                return false;
        for (u32 k = 0; k < kwarg; k++)
            if (!annotate_one(kid(n.a, at_kwdef + a.c + k), count))
                return false;
    }
    if (n.d) {
        StrObj *ret = str_intern("return");
        if (!ret)
            return oom();
        if (!emit(Bc::LoadConst, add_const(obj_value(ret)), n.d) || !expr(n.d))
            return false;
        count++;
    }
    return emit(Bc::BuildMap, count, node) && emit(Bc::Return, node);
}

// A class body or a module keeps its __annotate__ in its namespace; only a
// def carries it on the object.
bool Compiler::store_annotate(u32 node)
{
    u32 flags = 0;
    if (!annotate_of(node, flags))
        return false;
    if (!flags)
        return true;
    StrObj *key =
        str_intern(ast->at(node).kind == Nd::Module ? "__annotate__" : "__annotate_func__");
    if (!key)
        return oom();
    return emit(Bc::StoreName, name_index(key), 0);
}

// The set a conditional annotation reports itself to. A class keeps it in a
// cell, so its __annotate__ sees it.
bool Compiler::conditional_set(u32 node)
{
    if (!st.scopes[u->scope].nconds)
        return true;
    StrObj *conds = str_intern("__conditional_annotations__");
    if (!conds)
        return oom();
    if (!emit(Bc::BuildSet, 0, node))
        return false;
    if (scope().condcell) {
        const Sym *y = st.find(u->scope, conds);
        if (!y)
            return fail("the conditional annotation cell is missing", node);
        return emit(Bc::StoreDeref, y->slot, 0);
    }
    return emit(Bc::StoreName, name_index(conds), 0);
}

// A call to the scope of `i`'s type parameters, with `flags`' defaults on the
// stack as its arguments; what it answers is the function.
bool Compiler::generic_function(u32 i, u32 flags)
{
    u32 scope = st.anno(i, AN_PARAMS);
    if (!scope)
        return fail("an annotation scope is missing", i);
    u32 nargs = u32(__builtin_popcount(flags & (MF_DEFAULTS | MF_KWDEFAULTS)));
    Buf<128> b;
    b.put("<generic parameters of ").put(ast->text(i)).put(">");
    StrObj *name = str_intern(b.str());
    if (!name)
        return oom();
    u32 cl = 0;
    if (!closure_of_scope(scope, i, cl))
        return false;
    u32 k = nested_scope(scope, i, name, AN_PARAMS);
    if (failed || !emit(Bc::LoadConst, k, i) || !emit(Bc::MakeFunction, cl, i))
        return false;
    if (nargs == 1 && !emit(Bc::RotTwo, i))
        return false;
    if (nargs == 2 && !emit(Bc::RotThree, i))
        return false;
    return emit(Bc::Call, nargs, i);
}

bool Compiler::generic_class(u32 i)
{
    u32 scope = st.anno(i, AN_PARAMS);
    if (!scope)
        return fail("an annotation scope is missing", i);
    Buf<128> b;
    b.put("<generic parameters of ").put(ast->text(i)).put(">");
    StrObj *name = str_intern(b.str());
    if (!name)
        return oom();
    u32 cl = 0;
    if (!closure_of_scope(scope, i, cl))
        return false;
    u32 k = nested_scope(scope, i, name, AN_PARAMS);
    return !failed && emit(Bc::LoadConst, k, i) && emit(Bc::MakeFunction, cl, i) &&
           emit(Bc::Call, 0, i);
}

bool Compiler::type_alias(u32 i)
{
    const Node &n = ast->at(i);
    StrObj *raw   = raw_ident(n.a);
    StrObj *bound = ident(n.a);
    if (!raw || !bound)
        return oom();
    if (n.c) {
        u32 scope = st.anno(i, AN_PARAMS);
        if (!scope)
            return fail("an annotation scope is missing", i);
        Buf<128> b;
        b.put("<generic parameters of ").put(raw->str()).put(">");
        StrObj *name = str_intern(b.str());
        if (!name)
            return oom();
        u32 cl = 0;
        if (!closure_of_scope(scope, i, cl))
            return false;
        u32 k = nested_scope(scope, i, name, AN_PARAMS);
        if (failed || !emit(Bc::LoadConst, k, i) || !emit(Bc::MakeFunction, cl, i) ||
            !emit(Bc::Call, 0, i))
            return false;
    } else if (!emit(Bc::LoadConst, add_const(obj_value(raw)), i) ||
               !emit(Bc::LoadConst, const_none(), i) || !anno_function(i, AN_VALUE, raw) ||
               !emit(Bc::BuildTuple, 3, i) || !emit(Bc::Intrinsic, TI_TYPEALIAS, i)) {
        return false;
    }
    return store_name(bound, n.a);
}

// -------------------------------------------------------- try, except, with

bool Compiler::try_except(u32 i)
{
    const Node &n = ast->at(i);
    u32 handlers  = emit_jump(Bc::SetupFinally, i);

    // The body is a block of its own: a `break` out of it has to pop the
    // handler, or the next exception lands in a dead one.
    FBlock tb;
    tb.kind = FK::Try;
    tb.node = i;
    if (!block_push(tb))
        return false;
    bool body_ok = stmts(i, 0, n.a);
    u->blocks.pop();
    if (!body_ok || !emit(Bc::PopBlock, i))
        return false;

    u32 to_else = emit_jump(Bc::Jump, i);
    patch(handlers);
    if (!emit(Bc::PushExcInfo, i))
        return false;

    Vec<u32> ends;
    u32 next = 0;
    for (u32 k = 0; k < n.b; k++) {
        if (next)
            patch(next);
        next          = 0;
        u32 h         = kid(i, n.a + k);
        const Node &x = ast->at(h);

        if (x.a) {
            if (!emit(Bc::DupTop, h) || !expr(x.a) || !emit(Bc::CheckExcMatch, h))
                return false;
            next = emit_jump(Bc::PopJumpIfFalse, h);
        }

        StrObj *name = (x.flags & 1) ? ident(h) : nullptr;
        if ((x.flags & 1) && !name)
            return oom();

        FBlock f;
        f.kind = FK::Handler;
        f.node = h;
        f.name = name;
        // Whatever the body raises puts back what was being handled first.
        if (!(name ? store_name(name, h) : emit(Bc::PopTop, h)))
            return false;
        u32 cleanup = emit_jump(Bc::SetupFinally, h);
        if (!block_push(f))
            return false;

        bool ok = stmts(h, 0, x.b);
        u->blocks.pop();
        if (!ok)
            return false;

        if (!emit(Bc::PopBlock, h))
            return false;
        if (name &&
            (!emit(Bc::LoadConst, const_none(), h) || !store_name(name, h) || !del_name(name, h)))
            return false;
        if (!emit(Bc::PopExcept, h))
            return false;
        if (!ends.push(emit_jump(Bc::Jump, h)))
            return oom();

        patch(cleanup);
        if (name &&
            (!emit(Bc::LoadConst, const_none(), h) || !store_name(name, h) || !del_name(name, h)))
            return false;
        if (!emit(Bc::Reraise, 1, h))
            return false;
    }
    if (next)
        patch(next);
    if (!emit(Bc::Reraise, 1, i))
        return false;

    patch(to_else);
    if (!stmts(i, n.a + n.b, n.c))
        return false;
    patch_all(ends);
    return !failed;
}

// except*, laid out as CPython's codegen does it, over this VM's blocks. The
// handler region starts with [prev, exc]: a list collects what each clause
// raises, `rest` is what no clause has taken yet, and at the end the two make
// what is raised again.
bool Compiler::try_star(u32 i)
{
    const Node &n = ast->at(i);
    u32 handlers  = emit_jump(Bc::SetupFinally, i);
    FBlock tb;
    tb.kind = FK::Try;
    tb.node = i;
    if (!block_push(tb))
        return false;
    bool body_ok = stmts(i, 0, n.a);
    u->blocks.pop();
    if (!body_ok || !emit(Bc::PopBlock, i))
        return false;
    u32 to_else = emit_jump(Bc::Jump, i);

    patch(handlers);
    if (!emit(Bc::PushExcInfo, i))
        return false;
    u32 cleanup = emit_jump(Bc::SetupFinally, i);
    FBlock star;
    star.kind = FK::StarBody;
    star.node = i;
    if (!block_push(star))
        return false;
    if (!emit(Bc::BuildList, 0, i) || !emit(Bc::Copy, 2, i))
        return false;

    u32 reraise_star = 0;
    for (u32 k = 0; k < n.b; k++) {
        u32 h         = kid(i, n.a + k);
        const Node &x = ast->at(h);
        // [prev, exc, list, rest]
        if (!expr(x.a) || !emit(Bc::CheckEgMatch, h) || !emit(Bc::Copy, 1, h) ||
            !emit(Bc::LoadConst, const_none(), h) || !emit(Bc::CompareOp, u32(Cmp::Is), h))
            return false;
        u32 no_match = emit_jump(Bc::PopJumpIfTrue, h);

        StrObj *name = (x.flags & 1) ? ident(h) : nullptr;
        if ((x.flags & 1) && !name)
            return oom();
        if (!(name ? store_name(name, h) : emit(Bc::PopTop, h)))
            return false;

        u32 cleanup_end = emit_jump(Bc::SetupFinally, h);
        FBlock inner;
        inner.kind = FK::Try;
        inner.node = h;
        if (!block_push(inner))
            return false;
        bool ok = stmts(h, 0, x.b);
        u->blocks.pop();
        if (!ok || !emit(Bc::PopBlock, h))
            return false;
        if (name &&
            (!emit(Bc::LoadConst, const_none(), h) || !store_name(name, h) || !del_name(name, h)))
            return false;
        u32 next = emit_jump(Bc::Jump, h);

        // The clause raised: [prev, exc, list, rest, raised] goes on the list.
        patch(cleanup_end);
        if (name &&
            (!emit(Bc::LoadConst, const_none(), h) || !store_name(name, h) || !del_name(name, h)))
            return false;
        if (!emit(Bc::ListAppend, 2, h))
            return false;
        u32 with_error = emit_jump(Bc::Jump, h);

        patch(no_match);
        if (!emit(Bc::PopTop, h))
            return false;
        patch(next);
        patch(with_error);
        if (k == n.b - 1) {
            // What no clause took is raised again with the rest.
            if (!emit(Bc::ListAppend, 1, h))
                return false;
            reraise_star = here();
        }
    }
    u->blocks.pop();

    // [prev, exc, list] to [prev, result]
    (void)reraise_star;
    if (!emit(Bc::PrepReraiseStar, i) || !emit(Bc::Copy, 1, i) ||
        !emit(Bc::LoadConst, const_none(), i) || !emit(Bc::CompareOp, u32(Cmp::IsNot), i))
        return false;
    u32 reraise = emit_jump(Bc::PopJumpIfTrue, i);
    if (!emit(Bc::PopTop, i) || !emit(Bc::PopBlock, i) || !emit(Bc::PopExcept, i))
        return false;
    u32 end = emit_jump(Bc::Jump, i);

    patch(reraise);
    if (!emit(Bc::PopBlock, i) || !emit(Bc::Reraise, 1, i))
        return false;

    // Anything that fails on the way: the handled exception is put back.
    patch(cleanup);
    if (!emit(Bc::RotTwo, i) || !emit(Bc::PopTop, i) || !emit(Bc::Reraise, 1, i))
        return false;

    patch(to_else);
    if (!stmts(i, n.a + n.b, n.c))
        return false;
    patch(end);
    return !failed;
}

bool Compiler::try_stmt(u32 i)
{
    const Node &n = ast->at(i);
    bool star     = n.kind == Nd::TryStar;
    if (!n.d)
        return star ? try_star(i) : try_except(i);

    u32 fin = emit_jump(Bc::SetupFinally, i);
    FBlock f;
    f.kind = FK::Finally;
    f.node = i;
    if (!block_push(f))
        return false;
    bool ok = n.b ? (star ? try_star(i) : try_except(i)) : stmts(i, 0, n.a);
    u->blocks.pop();
    if (!ok)
        return false;

    if (!emit(Bc::PopBlock, i) || !stmts(i, n.a + n.b + n.c, n.d))
        return false;
    u32 end = emit_jump(Bc::Jump, i);
    patch(fin);
    // The second copy runs with the exception on the stack, waiting for the
    // Reraise below; an exit out of the clause has to drop that too.
    u->pending++;
    bool again = stmts(i, n.a + n.b + n.c, n.d);
    u->pending--;
    if (!again || !emit(Bc::Reraise, 0, i))
        return false;
    patch(end);
    return !failed;
}

bool Compiler::with_at(u32 i, u32 k)
{
    const Node &n = ast->at(i);
    if (k == n.a)
        return stmts(i, n.a, n.b);

    u32 item      = kid(i, k);
    const Node &x = ast->at(item);
    bool async    = n.kind == Nd::AsyncWith;
    if (!expr(x.a) || !emit(async ? Bc::BeforeAsyncWith : Bc::BeforeWith, item))
        return false;
    if (async && !await_top(item, AW_AENTER))
        return false;
    u32 fin = emit_jump(Bc::SetupWith, item);

    FBlock f;
    f.kind  = FK::With;
    f.node  = item;
    f.async = async;
    if (!block_push(f))
        return false;
    bool ok = (x.b ? store(x.b) : emit(Bc::PopTop, item)) && with_at(i, k + 1);
    u->blocks.pop();
    if (!ok)
        return false;

    if (!emit(Bc::PopBlock, item) || !emit(Bc::LoadConst, const_none(), item) ||
        !emit(Bc::DupTop, item) || !emit(Bc::DupTop, item) || !emit(Bc::Call, 3, item))
        return false;
    if (async && !await_top(item, AW_AEXIT))
        return false;
    if (!emit(Bc::PopTop, item))
        return false;
    u32 end = emit_jump(Bc::Jump, item);

    patch(fin);
    if (!emit(Bc::WithExceptStart, item))
        return false;
    if (async && !await_top(item, AW_AEXIT))
        return false;
    u32 swallow = emit_jump(Bc::PopJumpIfTrue, item);
    if (!emit(Bc::Reraise, 0, item))
        return false;
    patch(swallow);
    if (!emit(Bc::PopTop, item) || !emit(Bc::PopTop, item))
        return false;
    patch(end);
    return !failed;
}

// ------------------------------------------------------------------ match
//
// CPython's codegen.c, pattern for pattern: see PatCtx. A jump into
// fail_pop[n] pops n values on its way to the next case.

bool Compiler::jump_fail(PatCtx &pc, Bc op, u32 node)
{
    u32 pops = pc.on_top + u32(pc.stores.size());
    while (pc.fail_pop.size() <= pops)
        if (!pc.fail_pop.push(Vec<u32>()))
            return oom();
    u32 j = emit_jump(op, node);
    if (failed)
        return false;
    return pc.fail_pop[pops].push(j) ? true : oom();
}

bool Compiler::emit_fail_pop(PatCtx &pc)
{
    for (usize k = pc.fail_pop.size(); k > 1; k--) {
        patch_all(pc.fail_pop[k - 1]);
        if (!emit(Bc::PopTop, 0))
            return false;
    }
    if (pc.fail_pop.size())
        patch_all(pc.fail_pop[0]);
    pc.fail_pop.clear();
    return !failed;
}

bool Compiler::rotate(u32 count, u32 node)
{
    for (; count > 1; count--)
        if (!emit(Bc::Swap, count, node))
            return false;
    return true;
}

// A capture waits under what is on top, until the whole pattern matches.
bool Compiler::store_capture(StrObj *name, PatCtx &pc, u32 node)
{
    if (!name)
        return emit(Bc::PopTop, node);
    for (usize k = 0; k < pc.stores.size(); k++)
        if (pc.stores[k] == name) {
            Buf<128> b;
            b.put("multiple assignments to name '").put(name->str()).put("' in pattern");
            return fail(b.str(), node);
        }
    if (!rotate(pc.on_top + u32(pc.stores.size()) + 1, node))
        return false;
    return pc.stores.push(name) ? true : oom();
}

// The value on top is a tuple, or None for no match.
bool Compiler::not_none_or_fail(PatCtx &pc, u32 node)
{
    return emit(Bc::Copy, 1, node) && emit(Bc::LoadConst, const_none(), node) &&
           emit(Bc::CompareOp, u32(Cmp::IsNot), node) && jump_fail(pc, Bc::PopJumpIfFalse, node);
}

bool Compiler::subpattern(u32 p, PatCtx &pc)
{
    bool allow           = pc.allow_irrefutable;
    pc.allow_irrefutable = true;
    bool ok              = pattern(p, pc);
    pc.allow_irrefutable = allow;
    return ok;
}

bool Compiler::pattern_as(u32 p, PatCtx &pc)
{
    const Node &n = ast->at(p);
    StrObj *name  = (n.flags & 1) ? ident(p) : nullptr;
    if ((n.flags & 1) && !name)
        return oom();
    if (!n.a) {
        if (!pc.allow_irrefutable) {
            if (!name)
                return fail("wildcard makes remaining patterns unreachable", p);
            Buf<128> b;
            b.put("name capture '").put(ast->text(p)).put("' makes remaining patterns unreachable");
            return fail(b.str(), p);
        }
        return store_capture(name, pc, p);
    }
    pc.on_top++;
    if (!emit(Bc::Copy, 1, p) || !pattern(n.a, pc))
        return false;
    pc.on_top--;
    return store_capture(name, pc, p);
}

bool Compiler::pattern_sequence(u32 p, PatCtx &pc)
{
    const Node &n  = ast->at(p);
    u32 size       = n.nkid;
    i32 star       = -1;
    bool only_wild = true, star_wild = false;
    for (u32 k = 0; k < size; k++) {
        u32 e = kid(p, k);
        if (ast->at(e).kind == Nd::MatchStar) {
            if (star >= 0)
                return fail("multiple starred names in sequence pattern", p);
            star_wild = is_star_wildcard(e);
            only_wild = only_wild && star_wild;
            star      = i32(k);
            continue;
        }
        only_wild = only_wild && is_wildcard(e);
    }
    pc.on_top++;
    if (!emit(Bc::MatchSequence, p) || !jump_fail(pc, Bc::PopJumpIfFalse, p))
        return false;
    if (star < 0 || size > 1) {
        u32 want = star < 0 ? size : size - 1;
        Cmp op   = star < 0 ? Cmp::Eq : Cmp::Ge;
        if (!emit(Bc::GetLen, p) || !emit(Bc::LoadConst, add_const(Value::of_int(i32(want))), p) ||
            !emit(Bc::CompareOp, u32(op), p) || !jump_fail(pc, Bc::PopJumpIfFalse, p))
            return false;
    }
    pc.on_top--;
    if (only_wild)
        return emit(Bc::PopTop, p);

    if (star_wild) {
        // Only what is asked for is fetched, by index from either end.
        pc.on_top++;
        for (u32 k = 0; k < size; k++) {
            u32 e = kid(p, k);
            if (is_wildcard(e) || i32(k) == star)
                continue;
            if (!emit(Bc::Copy, 1, e))
                return false;
            if (i32(k) < star) {
                if (!emit(Bc::LoadConst, add_const(Value::of_int(i32(k))), e))
                    return false;
            } else if (!emit(Bc::GetLen, e) ||
                       !emit(Bc::LoadConst, add_const(Value::of_int(i32(size - k))), e) ||
                       !emit(Bc::BinaryOp, u32(Op::Sub), e)) {
                return false;
            }
            if (!emit(Bc::LoadSubscr, e) || !subpattern(e, pc))
                return false;
        }
        pc.on_top--;
        return emit(Bc::PopTop, p);
    }

    if (star >= 0) {
        if (u32(star) >= 0x10000 || size - u32(star) - 1 >= 0x10000)
            return fail("too many expressions in star-unpacking sequence pattern", p);
        if (!emit(Bc::UnpackEx, u32(star) | ((size - u32(star) - 1) << 16), p))
            return false;
    } else if (!emit(Bc::UnpackSequence, size, p)) {
        return false;
    }
    pc.on_top += size;
    for (u32 k = 0; k < size; k++) {
        pc.on_top--;
        if (!subpattern(kid(p, k), pc))
            return false;
    }
    return true;
}

bool Compiler::pattern_mapping(u32 p, PatCtx &pc)
{
    const Node &n = ast->at(p);
    u32 size      = n.a;
    bool rest     = (n.flags & 1) != 0;
    pc.on_top++;
    if (!emit(Bc::MatchMapping, p) || !jump_fail(pc, Bc::PopJumpIfFalse, p))
        return false;
    if (!size && !rest) {
        pc.on_top--;
        return emit(Bc::PopTop, p);
    }
    if (size &&
        (!emit(Bc::GetLen, p) || !emit(Bc::LoadConst, add_const(Value::of_int(i32(size))), p) ||
         !emit(Bc::CompareOp, u32(Cmp::Ge), p) || !jump_fail(pc, Bc::PopJumpIfFalse, p)))
        return false;

    // The keys: literals, which may not repeat, or attribute lookups.
    Vec<u32> seen;
    for (u32 k = 0; k < size; k++) {
        u32 key = kid(p, k);
        if (ast->at(key).kind == Nd::Attribute) {
            if (!expr(key))
                return false;
            continue;
        }
        Root v{ literal_of(key) };
        if (v.v.is_nil())
            return failed = true, false;
        for (usize j = 0; j < seen.size(); j++) {
            bool same = false;
            if (py_eq(co()->consts[seen[j]], v.v, same) != R::Ok)
                return failed = true, false;
            if (same) {
                String text;
                if (py_repr(v.v, text) != R::Ok)
                    return oom();
                Buf<128> b;
                b.put("mapping pattern checks duplicate key (").put(text.str()).put(")");
                return fail(b.str(), p);
            }
        }
        u32 at = add_const(v.v);
        if (!emit(Bc::LoadConst, at, key) || !seen.push(at))
            return failed ? false : oom();
    }
    if (!emit(Bc::BuildTuple, size, p) || !emit(Bc::MatchKeys, p))
        return false;
    pc.on_top += 2;
    if (!not_none_or_fail(pc, p) || !emit(Bc::UnpackSequence, size, p))
        return false;
    pc.on_top += size - 1;
    for (u32 k = 0; k < size; k++) {
        pc.on_top--;
        if (!subpattern(kid(p, size + k), pc))
            return false;
    }
    pc.on_top -= 2;
    if (!rest)
        return emit(Bc::PopTop, p) && emit(Bc::PopTop, p);

    // rest = dict(subject), less every key: [subject, keys] to [rest].
    if (!emit(Bc::Swap, 2, p) || !emit(Bc::CopyDict, p) || !emit(Bc::Swap, 2, p) ||
        !emit(Bc::UnpackSequence, size, p))
        return false;
    for (u32 left = size; left; left--)
        if (!emit(Bc::Copy, 1 + left, p) || !emit(Bc::Swap, 2, p) || !emit(Bc::DeleteSubscr, p))
            return false;
    StrObj *name = ident(p);
    return name ? store_capture(name, pc, p) : oom();
}

bool Compiler::pattern_class(u32 p, PatCtx &pc)
{
    const Node &n = ast->at(p);
    u32 nargs     = n.b;
    u32 nattrs    = n.nkid - n.b;
    Root names{ obj_value(tuple_new(nattrs)) };
    if (names.v.is_nil())
        return oom();
    for (u32 k = 0; k < nattrs; k++) {
        u32 w = kid(p, nargs + k);
        for (u32 j = k + 1; j < nattrs; j++)
            if (ast->text(kid(p, nargs + j)) == ast->text(w)) {
                Buf<128> b;
                b.put("attribute name repeated in class pattern: ").put(ast->text(w));
                return fail(b.str(), ast->at(kid(p, nargs + j)).a);
            }
        StrObj *s = raw_ident(w);
        if (!s)
            return oom();
        static_cast<TupleObj *>(names.v.obj())->items()[k] = obj_value(s);
    }
    if (!expr(n.a) || !emit(Bc::LoadConst, add_const(names.v), p) ||
        !emit(Bc::MatchClass, nargs, p))
        return false;
    pc.on_top++;
    if (!not_none_or_fail(pc, p) || !emit(Bc::UnpackSequence, nargs + nattrs, p))
        return false;
    pc.on_top += nargs + nattrs;
    pc.on_top--;
    for (u32 k = 0; k < nargs + nattrs; k++) {
        pc.on_top--;
        u32 sub = k < nargs ? kid(p, k) : ast->at(kid(p, k)).a;
        if (is_wildcard(sub)) {
            if (!emit(Bc::PopTop, p))
                return false;
            continue;
        }
        if (!subpattern(sub, pc))
            return false;
    }
    return true;
}

bool Compiler::pattern_or(u32 p, PatCtx &pc)
{
    const Node &n = ast->at(p);
    Vec<u32> ends;
    Vec<StrObj *> control;
    PatCtx outer;
    outer.on_top            = pc.on_top;
    outer.allow_irrefutable = pc.allow_irrefutable;
    outer.stores            = static_cast<Vec<StrObj *> &&>(pc.stores);
    outer.fail_pop          = static_cast<Vec<Vec<u32>> &&>(pc.fail_pop);

    for (u32 k = 0; k < n.nkid; k++) {
        u32 alt = kid(p, k);
        PatCtx one;
        one.allow_irrefutable = k == n.nkid - 1 && outer.allow_irrefutable;
        if (!emit(Bc::Copy, 1, alt) || !pattern(alt, one))
            return false;
        usize nstores = one.stores.size();
        if (!k) {
            for (usize j = 0; j < nstores; j++)
                if (!control.push(one.stores[j]))
                    return oom();
        } else if (nstores != control.size()) {
            return fail("alternative patterns bind different names", p);
        } else if (nstores) {
            for (usize ic = nstores; ic-- > 0;) {
                StrObj *name = control[ic];
                usize is     = nstores;
                for (usize j = 0; j < nstores; j++)
                    if (one.stores[j] == name)
                        is = j;
                if (is == nstores)
                    return fail("alternative patterns bind different names", p);
                if (is == ic)
                    continue;
                // Move stores[0 .. is] up to sit just below ic, on the stack
                // as in the list.
                usize rotations = is + 1;
                Vec<StrObj *> moved;
                for (usize j = 0; j < nstores; j++)
                    if (!moved.push(nullptr))
                        return oom();
                usize at = 0;
                for (usize j = rotations; j < rotations + (ic - is); j++)
                    moved[at++] = one.stores[j];
                for (usize j = 0; j < rotations; j++)
                    moved[at++] = one.stores[j];
                for (usize j = rotations + (ic - is); j < nstores; j++)
                    moved[at++] = one.stores[j];
                one.stores = static_cast<Vec<StrObj *> &&>(moved);
                for (usize r = 0; r < rotations; r++)
                    if (!rotate(u32(ic + 1), alt))
                        return false;
            }
        }
        u32 j = emit_jump(Bc::Jump, alt);
        if (failed || !ends.push(j))
            return failed ? false : oom();
        if (!emit_fail_pop(one))
            return false;
    }

    pc.on_top            = outer.on_top;
    pc.allow_irrefutable = outer.allow_irrefutable;
    pc.stores            = static_cast<Vec<StrObj *> &&>(outer.stores);
    pc.fail_pop          = static_cast<Vec<Vec<u32>> &&>(outer.fail_pop);
    // No alternative matched: drop the copy of the subject and fail.
    if (!emit(Bc::PopTop, p) || !jump_fail(pc, Bc::Jump, p))
        return false;

    patch_all(ends);
    u32 nstores = u32(control.size());
    u32 nrots   = nstores + 1 + pc.on_top + u32(pc.stores.size());
    for (u32 k = 0; k < nstores; k++) {
        if (!rotate(nrots, p))
            return false;
        for (usize j = 0; j < pc.stores.size(); j++)
            if (pc.stores[j] == control[k]) {
                Buf<128> b;
                b.put("multiple assignments to name '").put(control[k]->str());
                b.put("' in pattern");
                return fail(b.str(), p);
            }
        if (!pc.stores.push(control[k]))
            return oom();
    }
    return emit(Bc::PopTop, p);
}

bool Compiler::pattern(u32 p, PatCtx &pc)
{
    if (failed)
        return false;
    const Node &n = ast->at(p);
    switch (n.kind) {
    case Nd::MatchValue: {
        if (ast->at(n.a).kind != Nd::Attribute) {
            Root v{ literal_of(n.a) };
            if (v.v.is_nil())
                return failed = true, false;
            if (!emit(Bc::LoadConst, add_const(v.v), p))
                return false;
        } else if (!expr(n.a)) {
            return false;
        }
        return emit(Bc::CompareOp, u32(Cmp::Eq), p) && jump_fail(pc, Bc::PopJumpIfFalse, p);
    }
    case Nd::MatchSingleton: {
        Value v = Const(n.flags) == Const::None   ? value_none()
                  : Const(n.flags) == Const::True ? value_bool(true)
                                                  : value_bool(false);
        return emit(Bc::LoadConst, add_const(v), p) && emit(Bc::CompareOp, u32(Cmp::Is), p) &&
               jump_fail(pc, Bc::PopJumpIfFalse, p);
    }
    case Nd::MatchSequence:
        return pattern_sequence(p, pc);
    case Nd::MatchMapping:
        return pattern_mapping(p, pc);
    case Nd::MatchClass:
        return pattern_class(p, pc);
    case Nd::MatchStar: {
        StrObj *name = (n.flags & 1) ? ident(p) : nullptr;
        if ((n.flags & 1) && !name)
            return oom();
        return store_capture(name, pc, p);
    }
    case Nd::MatchAs:
        return pattern_as(p, pc);
    case Nd::MatchOr:
        return pattern_or(p, pc);
    default:
        return fail("invalid match pattern", p);
    }
}

bool Compiler::match_stmt(u32 i)
{
    const Node &n = ast->at(i);
    if (!expr(n.a))
        return false;
    Vec<u32> ends;
    u32 cases        = n.nkid;
    u32 last         = kid(i, cases - 1);
    bool has_default = is_wildcard(ast->at(last).a) && cases > 1;
    for (u32 k = 0; k < cases - (has_default ? 1 : 0); k++) {
        u32 m         = kid(i, k);
        const Node &c = ast->at(m);
        bool copy     = k != cases - (has_default ? 1 : 0) - 1;
        if (copy && !emit(Bc::Copy, 1, c.a))
            return false;
        PatCtx pc;
        pc.allow_irrefutable = c.b || k == cases - 1;
        if (!pattern(c.a, pc))
            return false;
        // It matched: the captures are on the stack, in the order stored.
        for (usize j = 0; j < pc.stores.size(); j++)
            if (!store_name(pc.stores[j], c.a))
                return false;
        if (c.b) {
            if (!pc.fail_pop.size() && !pc.fail_pop.push(Vec<u32>()))
                return oom();
            if (!expr(c.b))
                return false;
            u32 j = emit_jump(Bc::PopJumpIfFalse, c.b);
            if (failed || !pc.fail_pop[0].push(j))
                return failed ? false : oom();
        }
        if (copy && !emit(Bc::PopTop, c.a))
            return false;
        if (!stmts(m, 0, c.nkid))
            return false;
        u32 j = emit_jump(Bc::Jump, m);
        if (failed || !ends.push(j))
            return failed ? false : oom();
        if (!emit_fail_pop(pc))
            return false;
    }
    if (has_default) {
        const Node &c = ast->at(last);
        if (!emit(Bc::Nop, c.a))
            return false;
        if (c.b) {
            if (!expr(c.b))
                return false;
            u32 j = emit_jump(Bc::PopJumpIfFalse, c.b);
            if (failed || !ends.push(j))
                return failed ? false : oom();
        }
        if (!stmts(last, 0, c.nkid))
            return false;
    }
    patch_all(ends);
    return !failed;
}

// -------------------------------------------------------------- statements

bool Compiler::stmt(u32 i)
{
    if (failed)
        return false;
    if (!i)
        return true;
    const Node &n = ast->at(i);

    switch (n.kind) {
    case Nd::Pass:
    case Nd::Global:
    case Nd::Nonlocal:
        return true;

    case Nd::Expr:
        // Single mode is the REPL's. The value of a statement is printed
        // rather than dropped, at the top level only, as CPython does.
        if (print_top && !u->prev)
            return expr(n.a) && emit(Bc::PrintExpr, i);
        return expr(n.a) && emit(Bc::PopTop, i);

    case Nd::Assign:
        if (!expr(n.a))
            return false;
        for (u32 k = 0; k < n.nkid; k++) {
            if (k + 1 < n.nkid && !emit(Bc::DupTop, i))
                return false;
            if (!store(kid(i, k)))
                return false;
        }
        return true;

    case Nd::AugAssign: {
        const Node &t = ast->at(n.a);
        switch (t.kind) {
        case Nd::Name: {
            StrObj *s = ident(n.a);
            return s && load_name(s, n.a) && expr(n.b) && emit(Bc::InplaceOp, n.flags, i) &&
                   store_name(s, n.a);
        }
        case Nd::Attribute: {
            StrObj *s = ident(n.a);
            return s && note_static(n.a) && expr(t.a) && emit(Bc::DupTop, i) &&
                   emit(Bc::LoadAttr, name_index(s), n.a) && expr(n.b) &&
                   emit(Bc::InplaceOp, n.flags, i) && emit(Bc::RotTwo, i) &&
                   emit(Bc::StoreAttr, name_index(s), n.a);
        }
        case Nd::Subscript:
            return expr(t.a) && expr(t.b) && emit(Bc::DupTop2, i) && emit(Bc::LoadSubscr, n.a) &&
                   expr(n.b) && emit(Bc::InplaceOp, n.flags, i) && emit(Bc::RotThree, i) &&
                   emit(Bc::StoreSubscr, n.a);
        default:
            return fail("cannot assign to this", n.a);
        }
    }

    // The annotation is the __annotate__ function's; all that happens here is
    // the assignment, and a note that a conditional annotation was reached.
    case Nd::AnnAssign: {
        if (n.c && !(expr(n.c) && store(n.a)))
            return false;
        if (!(n.flags & 1) || !scope().nconds)
            return true;
        u32 owner = u->scope;
        for (usize k = 0; k < st.scopes[owner].deferred.size(); k++) {
            if (st.scopes[owner].deferred[k] != i || st.scopes[owner].cond[k] < 0)
                continue;
            StrObj *conds = str_intern("__conditional_annotations__");
            if (!conds)
                return oom();
            i32 idx = st.scopes[owner].cond[k];
            if (!load_name(conds, i) || !emit(Bc::LoadConst, add_const(Value::of_int(idx)), i) ||
                !emit(Bc::SetAdd, 1, i) || !emit(Bc::PopTop, i))
                return false;
            break;
        }
        return true;
    }

    case Nd::Delete:
        for (u32 k = 0; k < n.nkid; k++)
            if (!del(kid(i, k)))
                return false;
        return true;

    case Nd::If: {
        if (!expr(n.a))
            return false;
        u32 other = emit_jump(Bc::PopJumpIfFalse, i);
        if (!stmts(i, 0, n.b))
            return false;
        if (!n.c) {
            patch(other);
            return !failed;
        }
        u32 end = emit_jump(Bc::Jump, i);
        patch(other);
        if (!stmts(i, n.b, n.c))
            return false;
        patch(end);
        return !failed;
    }

    case Nd::While: {
        u32 top = here();
        if (!expr(n.a))
            return false;
        u32 out = emit_jump(Bc::PopJumpIfFalse, i);

        FBlock f;
        f.kind = FK::Loop;
        f.cont = top;
        if (!block_push(f))
            return false;
        bool ok         = stmts(i, 0, n.b) && emit(Bc::Jump, top, i);
        Vec<u32> breaks = static_cast<Vec<u32> &&>(u->blocks.back().breaks);
        u->blocks.pop();
        if (!ok)
            return false;

        patch(out);
        if (!stmts(i, n.b, n.c))
            return false;
        patch_all(breaks);
        return !failed;
    }

    case Nd::For:
    case Nd::AsyncFor: {
        bool async = n.kind == Nd::AsyncFor;
        if (!expr(n.b) || !emit(async ? Bc::GetAIter : Bc::GetIter, i))
            return false;
        u32 top = here();
        u32 out = async ? async_next(i) : emit_jump(Bc::ForIter, i);
        if (failed)
            return false;

        FBlock f;
        f.kind = FK::Loop;
        f.cont = top;
        f.pops = 1;
        if (!block_push(f))
            return false;
        bool ok         = store(n.a) && stmts(i, 0, n.c) && emit(Bc::Jump, top, i);
        Vec<u32> breaks = static_cast<Vec<u32> &&>(u->blocks.back().breaks);
        u->blocks.pop();
        if (!ok)
            return false;

        // An async for ends when its handler sees StopAsyncIteration.
        patch(out);
        if (async && !emit(Bc::EndAsyncFor, i))
            return false;
        if (!stmts(i, n.c, n.d))
            return false;
        patch_all(breaks);
        return !failed;
    }

    case Nd::Break:
        return loop_exit(true, i);
    case Nd::Continue:
        return loop_exit(false, i);

    case Nd::Return:
        if (!(n.a ? expr(n.a) : emit(Bc::LoadConst, const_none(), i)))
            return false;
        return unwind(0, true) && emit(Bc::Return, i);

    case Nd::Raise:
        if (!n.a)
            return emit(Bc::Raise, 0, i);
        if (!expr(n.a))
            return false;
        if (!n.b)
            return emit(Bc::Raise, 1, i);
        return expr(n.b) && emit(Bc::Raise, 2, i);

    case Nd::Assert: {
        if (level() >= 1)
            return true;
        if (!expr(n.a))
            return false;
        u32 ok = emit_jump(Bc::PopJumpIfTrue, i);
        if (!emit(Bc::LoadAssertionError, i))
            return false;
        if (n.b && (!expr(n.b) || !emit(Bc::Call, 1, i)))
            return false;
        if (!emit(Bc::Raise, 1, i))
            return false;
        patch(ok);
        return !failed;
    }

    case Nd::Try:
    case Nd::TryStar:
        return try_stmt(i);
    case Nd::With:
    case Nd::AsyncWith:
        return with_at(i, 0);

    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef:
        return function(i, true);
    case Nd::ClassDef:
        return classdef(i);

    case Nd::Import:
        return import(i);
    case Nd::ImportFrom:
        return import_from(i);

    case Nd::Match:
        return match_stmt(i);
    case Nd::TypeAlias:
        return type_alias(i);

    default:
        return fail("this statement is not compiled yet", i);
    }
}

// ------------------------------------------------------------ code objects

// What a nested scope's body compiles to, once its unit is current.
bool Compiler::body_of(u32 node)
{
    const Node &n  = ast->at(node);
    const Scope &s = scope();

    // A parameter some nested scope captures lives in a cell as well as in its
    // argument slot, so the frame copies it across on entry.
    for (usize k = 0; k < s.cellvars.size(); k++) {
        const Sym &y = s.syms[s.cellvars[k]];
        if (y.param &&
            (!emit(Bc::LoadFast, y.param - 1, node) || !emit(Bc::StoreDeref, u32(k), node)))
            return false;
    }

    switch (n.kind) {
    case Nd::FunctionDef:
    case Nd::AsyncFunctionDef:
        if (!stmts(node, 0, n.b))
            return false;
        break;

    case Nd::Lambda:
        return expr(n.b) && emit(Bc::Return, node);

    case Nd::ClassDef:
        if (!class_preamble(node) || !stmts(node, n.a + n.b, n.c) || !store_annotate(node) ||
            !store_statics())
            return false;
        // The body answers the __class__ cell, and leaves it where
        // type.__new__ looks; __build_class__ checks it was filled.
        if (scope().classcell) {
            const Sym *y = st.find(u->scope, str_intern("__class__"));
            StrObj *cc   = str_intern("__classcell__");
            if (!y || !cc)
                return y ? oom() : fail("the __class__ cell is missing", node);
            return emit(Bc::LoadClosure, y->slot, 0) && emit(Bc::StoreName, name_index(cc), 0) &&
                   emit(Bc::LoadClosure, y->slot, 0) && emit(Bc::Return, 0);
        }
        break;

    case Nd::ListComp:
    case Nd::SetComp:
    case Nd::DictComp:
    case Nd::GeneratorExp: {
        bool gen = n.kind == Nd::GeneratorExp;
        if (!gen) {
            Bc mk = n.kind == Nd::ListComp  ? Bc::BuildList
                    : n.kind == Nd::SetComp ? Bc::BuildSet
                                            : Bc::BuildMap;
            if (!emit(mk, 0, node))
                return false;
        }
        if (!emit(Bc::LoadFast, 0, node))
            return false;

        Vec<u32> tops, outs;
        for (u32 k = 0; k < n.nkid; k++) {
            u32 c         = kid(node, k);
            const Node &g = ast->at(c);
            bool async    = (g.flags & 1) != 0;
            if (k && (!expr(g.b) || !emit(async ? Bc::GetAIter : Bc::GetIter, c)))
                return false;
            if (!tops.push(here()))
                return oom();
            u32 out = async ? async_next(c) : emit_jump(Bc::ForIter, c);
            if (failed)
                return false;
            if (!outs.push(out))
                return oom();
            if (!store(g.a))
                return false;
            for (u32 j = 0; j < g.nkid; j++) {
                if (!expr(ast->kids[g.kid0 + j]))
                    return false;
                if (!emit(Bc::PopJumpIfFalse, tops.back(), c))
                    return false;
            }
        }

        u32 depth = n.nkid + 1;
        if (gen) {
            if (!expr(n.a) || (async_gen() && !emit(Bc::AsyncGenWrap, node)) ||
                !emit(Bc::YieldValue, node) || !emit(Bc::PopTop, node))
                return false;
        } else if (n.kind == Nd::DictComp) {
            if (!expr(n.a) || !expr(n.b) || !emit(Bc::MapAdd, depth, node))
                return false;
        } else if (!expr(n.a) ||
                   !emit(n.kind == Nd::ListComp ? Bc::ListAppend : Bc::SetAdd, depth, node)) {
            return false;
        }

        for (u32 k = n.nkid; k > 0; k--) {
            if (!emit(Bc::Jump, tops[k - 1], node))
                return false;
            patch(outs[k - 1]);
            if ((ast->at(kid(node, k - 1)).flags & 1) && !emit(Bc::EndAsyncFor, node))
                return false;
        }
        if (gen && !emit(Bc::LoadConst, const_none(), 0))
            return false;
        return emit(Bc::Return, 0);
    }

    default:
        return fail("this scope is not compiled yet", node);
    }

    // The implicit return keeps the line the body ended on.
    return emit(Bc::LoadConst, const_none(), 0) && emit(Bc::Return, 0);
}

Str scope_name(Nd kind)
{
    switch (kind) {
    case Nd::Lambda:
        return "<lambda>";
    case Nd::ListComp:
        return "<listcomp>";
    case Nd::SetComp:
        return "<setcomp>";
    case Nd::DictComp:
        return "<dictcomp>";
    case Nd::GeneratorExp:
        return "<genexpr>";
    default:
        return Str();
    }
}

// The stack an unpacked instruction leaves behind, on the path that falls
// through to the next one.
i32 effect(Bc op, u32 arg)
{
    switch (op) {
    case Bc::PopTop:
    case Bc::StoreName:
    case Bc::StoreFast:
    case Bc::StoreGlobal:
    case Bc::StoreDeref:
    case Bc::LoadSubscr:
    case Bc::BinaryOp:
    case Bc::InplaceOp:
    case Bc::CompareOp:
    case Bc::PopJumpIfFalse:
    case Bc::PopJumpIfTrue:
    case Bc::JumpIfFalseOrPop:
    case Bc::JumpIfTrueOrPop:
    case Bc::DeleteAttr:
    case Bc::ListAppend:
    case Bc::SetAdd:
    case Bc::ListExtend:
    case Bc::SetUpdate:
    case Bc::DictUpdate:
    case Bc::DictMerge:
    case Bc::ImportName:
    case Bc::ImportNameEager:
    case Bc::LazyImportName:
    case Bc::ImportStar:
    case Bc::PopExcept:
    case Bc::CheckExcMatch:
    case Bc::YieldFrom:
    case Bc::PrintExpr:
    case Bc::Return:
        return -1;
    case Bc::EndAsyncFor:
        return -2;

    case Bc::LoadConst:
    case Bc::LoadName:
    case Bc::LoadFast:
    case Bc::LoadGlobal:
    case Bc::LoadDeref:
    case Bc::LoadClosure:
    case Bc::LoadBuildClass:
    case Bc::LoadAssertionError:
    case Bc::DupTop:
    case Bc::ForIter:
    case Bc::ImportFrom:
    case Bc::PushExcInfo:
    case Bc::BeforeWith:
    case Bc::BeforeAsyncWith:
    case Bc::WithExceptStart:
    case Bc::GetANext:
    case Bc::Copy:
    case Bc::GetLen:
    case Bc::MatchSequence:
    case Bc::MatchMapping:
    case Bc::MatchKeys:
        return 1;
    case Bc::MatchClass:
        return -2;
    case Bc::PrepReraiseStar:
    case Bc::BuildTemplate:
        return -1;
    case Bc::BuildInterpolation:
        return (arg & FV_SPEC) ? -2 : -1;
    case Bc::Intrinsic:
        return 1 - i32(intrinsic_arity(arg));
    case Bc::LoadLocals:
        return 1;

    case Bc::DupTop2:
        return 2;
    case Bc::StoreAttr:
    case Bc::DeleteSubscr:
    case Bc::MapAdd:
        return -2;
    case Bc::StoreSubscr:
        return -3;

    case Bc::BuildTuple:
    case Bc::BuildList:
    case Bc::BuildSet:
    case Bc::BuildSlice:
        return 1 - i32(arg);
    case Bc::BuildMap:
        return 1 - 2 * i32(arg);
    case Bc::UnpackSequence:
        return i32(arg) - 1;
    case Bc::UnpackEx:
        return i32(arg & 0xffff) + i32(arg >> 16);

    case Bc::Call:
        return -i32(arg);
    case Bc::CallKw:
        return -i32(arg) - 1;
    case Bc::CallEx:
        return (arg & CX_KWARGS) ? -2 : -1;
    case Bc::MakeFunction:
        return -i32(__builtin_popcount(arg));
    case Bc::Raise:
        return -i32(arg);

    default:
        return 0;
    }
}

bool terminal(Bc op)
{
    return op == Bc::Return || op == Bc::Raise || op == Bc::Reraise;
}

// A depth-first walk over the instruction graph. Nothing runs yet, so this is
// only the frame's upper bound; a merge reached deeper is walked again.
u32 stack_size(const CodeObj *c)
{
    usize n = c->code.size();
    Vec<i32> depth;
    Vec<u32> work;
    if (!n || !depth.resize(n))
        return 0;
    for (usize k = 0; k < n; k++)
        depth[k] = -1;
    depth[0] = 0;
    if (!work.push(0))
        return 0;

    i32 most    = 0;
    usize steps = 0;
    while (work.size() && steps++ < 8 * n + 64) {
        u32 pc = work.back();
        work.pop();
        i32 d = depth[pc];

        for (;;) {
            if (d > most)
                most = d;
            const Instr &in = c->code[pc];

            if (bc_arg(in.op) == Arg::Jump) {
                i32 td = d;
                if (in.op == Bc::SetupFinally)
                    td = d + 1;
                else if (in.op == Bc::SetupWith)
                    td = d;
                else if (in.op == Bc::ForIter)
                    td = d - 1;
                else if (in.op == Bc::PopJumpIfFalse || in.op == Bc::PopJumpIfTrue)
                    td = d - 1;
                if (in.arg < n && td > depth[in.arg]) {
                    depth[in.arg] = td;
                    if (!work.push(in.arg))
                        return u32(most);
                }
            }
            if (in.op == Bc::Jump || terminal(in.op))
                break;

            d += effect(in.op, in.arg);
            pc++;
            if (pc >= n)
                break;
            if (d <= depth[pc])
                break;
            depth[pc] = d;
        }
    }
    return u32(most < 0 ? 0 : most);
}

// A scope's dotted path. A name defined in a function body has `<locals>`
// between the two, which is what CPython's __qualname__ says.
Value Compiler::qualname_of(StrObj *name)
{
    // The scope of a generic's type parameters is not part of the path.
    Unit *p = u;
    while (p && p->role != NO_ROLE)
        p = p->prev;
    if (!p || !p->prev)
        return obj_value(name);
    Root rn{ obj_value(name) };
    String b;
    Value outer = code_of(p->code.v)->qualname;
    if (!is_str(outer) || !b.append(str_of(outer)->str()))
        return oom(), Value();
    ScopeKind k = st.scopes[p->scope].kind;
    if (k != ScopeKind::Class && !b.append(".<locals>"))
        return oom(), Value();
    if (!b.push('.') || !b.append(str_of(rn.v)->str()))
        return oom(), Value();
    Value v = str_new(b.str());
    return v.is_nil() ? (oom(), Value()) : v;
}

// The docstring of a body. It is the first statement, when that statement is
// a plain string literal. -OO drops it.
Value Compiler::docstring(u32 node)
{
    if (level() >= 2)
        return Value();
    const Node &n = ast->at(node);
    u32 first     = 0;
    if (n.kind == Nd::Module && n.nkid)
        first = ast->kids[n.kid0];
    else if ((n.kind == Nd::FunctionDef || n.kind == Nd::AsyncFunctionDef) && n.b)
        first = ast->kids[n.kid0];
    else if (n.kind == Nd::ClassDef && n.c)
        first = ast->kids[n.kid0 + n.a + n.b];
    if (!first)
        return Value();
    const Node &s = ast->at(first);
    if (s.kind != Nd::Expr || !s.a)
        return Value();
    const Node &e = ast->at(s.a);
    if (e.kind != Nd::Constant || Const(e.flags) != Const::Str)
        return Value();
    return obj_value(str_raw(ast->lex.text_of(ast->lex.tokens[e.tok])));
}

// A module and a class body keep their docstring in the namespace, which is
// where __doc__ is read from. A function keeps it on the code object instead.
bool Compiler::store_doc()
{
    Root doc{ code_of(u->code.v)->doc };
    StrObj *key = str_intern("__doc__");
    if (!key)
        return oom();
    if (doc.v.is_nil())
        doc = value_none();
    return emit(Bc::LoadConst, add_const(doc.v), 0) && emit(Bc::StoreName, name_index(key), 0);
}

// What CPython puts in a class namespace before the body runs: the module, the
// qualified name, the first line with the decorators, and a docstring only
// where there is one.
bool Compiler::class_preamble(u32 node)
{
    const Node &n  = ast->at(node);
    StrObj *name   = str_intern("__name__");
    StrObj *module = str_intern("__module__");
    StrObj *qual   = str_intern("__qualname__");
    StrObj *first  = str_intern("__firstlineno__");
    StrObj *doc    = str_intern("__doc__");
    if (!name || !module || !qual || !first || !doc)
        return oom();
    u32 line = line_of(node);
    for (u32 k = 0; k < n.d; k++) {
        u32 at = line_of(kid(node, n.a + n.b + n.c + k)) - 1; // the `@` is on the line
        if (at && at < line)
            line = at;
    }
    CodeObj *c = co();
    Root q{ c->qualname };
    Root d{ c->doc };
    if (!emit(Bc::LoadName, name_index(name), 0) || !emit(Bc::StoreName, name_index(module), 0) ||
        !emit(Bc::LoadConst, add_const(q.v), 0) || !emit(Bc::StoreName, name_index(qual), 0) ||
        !emit(Bc::LoadConst, add_const(Value::of_int(i32(line))), 0) ||
        !emit(Bc::StoreName, name_index(first), 0))
        return false;
    // A generic class says what its parameters are.
    if (n.pad) {
        StrObj *tps = str_intern(".type_params");
        StrObj *tpn = str_intern("__type_params__");
        if (!tps || !tpn)
            return oom();
        if (!load_name(tps, 0) || !emit(Bc::StoreName, name_index(tpn), 0))
            return false;
    }
    // An annotation scope inside looks in this namespace, through a cell.
    if (scope().classdict) {
        const Sym *y = st.find(u->scope, str_intern("__classdict__"));
        if (!y || !emit(Bc::LoadLocals, 0) || !emit(Bc::StoreDeref, y->slot, 0))
            return y ? false : fail("the class namespace cell is missing", node);
    }
    if (!conditional_set(node))
        return false;
    if (d.v.is_nil())
        return true;
    return emit(Bc::LoadConst, add_const(d.v), 0) && emit(Bc::StoreName, name_index(doc), 0);
}

// `self.X = ...`: X is one of the nearest enclosing class's static attributes.
bool Compiler::note_static(u32 attr)
{
    u32 obj = ast->at(attr).a;
    if (ast->at(obj).kind != Nd::Name || ast->text(obj) != "self")
        return true;
    Unit *c = u->prev;
    while (c && st.scopes[c->scope].kind != ScopeKind::Class)
        c = c->prev;
    StrObj *s = raw_ident(attr);
    if (!c || !s)
        return s ? true : oom();
    for (usize k = 0; k < c->statics.size(); k++)
        if (c->statics[k] == s)
            return true;
    return c->statics.push(s) ? true : oom();
}

// The class body's last act: those names, sorted, as __static_attributes__.
bool Compiler::store_statics()
{
    Vec<StrObj *> &v = u->statics;
    for (usize i = 1; i < v.size(); i++)
        for (usize j = i; j > 0 && str_before(v[j]->str(), v[j - 1]->str()); j--) {
            StrObj *t = v[j];
            v[j]      = v[j - 1];
            v[j - 1]  = t;
        }
    Root names{ obj_value(tuple_new(v.size())) };
    StrObj *key = str_intern("__static_attributes__");
    if (names.v.is_nil() || !key)
        return oom();
    for (usize i = 0; i < v.size(); i++)
        static_cast<TupleObj *>(names.v.obj())->items()[i] = obj_value(v[i]);
    return emit(Bc::LoadConst, add_const(names.v), 0) && emit(Bc::StoreName, name_index(key), 0);
}

// One nested scope, compiled into its own code object and left in this unit's
// constants. The Root lives in `nu`, which is why this is a function.
u32 Compiler::nested(u32 node)
{
    const Node &n = ast->at(node);
    Str given     = scope_name(n.kind);
    StrObj *name  = str_intern(given.empty() ? ast->text(node) : given);
    if (!name)
        return oom(), 0;
    return nested_scope(st.at_node[node], node, name, NO_ROLE);
}

// A scope compiled into a code object left in this unit's constants: the
// node's own, or one of the annotation scopes `role` names.
u32 Compiler::nested_scope(u32 scope, u32 node, StrObj *name, u8 role)
{
    Unit nu;
    nu.prev  = u;
    nu.scope = scope;
    nu.role  = role;
    nu.code  = obj_value(code_new(obj_value(name), filename.v, line_of(node)));
    if (nu.code.v.is_nil())
        return oom(), 0;

    const Scope &s = st.scopes[nu.scope];
    CodeObj *c     = code_of(nu.code.v);
    c->qualname    = qualname_of(name);
    c->doc         = role == NO_ROLE ? docstring(node) : Value();
    if (c->qualname.is_nil())
        return oom(), 0;
    for (usize k = 0; k < s.varnames.size(); k++)
        if (!c->varnames.push(obj_value(s.syms[s.varnames[k]].name)))
            return oom(), 0;
    for (usize k = 0; k < s.cellvars.size(); k++)
        if (!c->cellvars.push(obj_value(s.syms[s.cellvars[k]].name)))
            return oom(), 0;
    for (usize k = 0; k < s.freevars.size(); k++)
        if (!c->freevars.push(obj_value(s.syms[s.freevars[k]].name)))
            return oom(), 0;
    c->argcount = s.argcount;
    c->posonly  = s.posonly;
    c->kwonly   = s.kwonly;
    u32 body    = !s.coroutine  ? (s.generator ? CO_GENERATOR : 0)
                  : s.generator ? CO_ASYNC_GENERATOR
                                : CO_COROUTINE;
    c->flags    = (s.varargs ? CO_VARARGS : 0) | (s.varkw ? CO_VARKW : 0) | body |
                  (s.freevars.size() ? CO_NESTED : 0) |
                  (s.kind == ScopeKind::Class ? 0 : CO_OPTIMIZED | CO_NEWLOCALS);

    u       = &nu;
    nu.line = c->firstline;
    bool ok = role == NO_ROLE ? body_of(node) : anno_body(node, role);
    u       = nu.prev;
    if (!ok)
        return 0;
    c->stacksize = stack_size(c);
    c->nblocks   = nu.blocks_max;
    return add_const(nu.code.v);
}

} // namespace

// Eval mode wants a tree of exactly one expression statement.
u32 lone_expression(const Ast &ast)
{
    const Node &root = ast.at(ast.root);
    if (root.nkid != 1)
        return 0;
    u32 first     = ast.kids[root.kid0];
    const Node &s = ast.at(first);
    return s.kind == Nd::Expr ? s.a : 0;
}

void compile_set_optimize(u32 l)
{
    opt_default = l;
}

u32 compile_optimize()
{
    return opt_default;
}

void compile_level_for(i32 l)
{
    opt_override = l;
}

Value py_compile(const Ast &ast, Str filename, CompileMode mode)
{
    py_init();

    Compiler c;
    c.ast       = &ast;
    c.print_top = mode == CompileMode::Single;
    if (!c.st.build(ast))
        return Value();

    StrObj *fn = str_intern(filename);
    StrObj *nm = str_intern("<module>");
    if (!fn || !nm)
        return err_set("MemoryError", "out of memory"), Value();
    c.filename = obj_value(fn);

    Unit mu;
    mu.scope = 0;
    mu.code  = obj_value(code_new(obj_value(nm), c.filename.v, 1));
    if (mu.code.v.is_nil())
        return err_set("MemoryError", "out of memory"), Value();
    c.u     = &mu;
    mu.line = 1;

    if (mode == CompileMode::Eval) {
        u32 e = lone_expression(ast);
        if (!e)
            return err_set_at("SyntaxError", "invalid syntax", 1, 0), Value();
        if (!c.expr(e) || !c.emit(Bc::Return, e))
            return Value();
        CodeObj *only   = code_of(mu.code.v);
        only->stacksize = stack_size(only);
        only->nblocks   = mu.blocks_max;
        return mu.code.v;
    }

    const Node &root        = ast.at(ast.root);
    code_of(mu.code.v)->doc = c.docstring(ast.root);
    // A module's __annotate__ goes in before the body, not after it, so that
    // the body itself can read the annotations of the lines above it.
    if (!c.store_annotate(ast.root) || !c.conditional_set(ast.root) || !c.store_doc() ||
        !c.stmts(ast.root, 0, root.nkid))
        return Value();
    if (!c.emit(Bc::LoadConst, c.const_none(), 0) || !c.emit(Bc::Return, 0))
        return Value();

    CodeObj *code   = code_of(mu.code.v);
    code->stacksize = stack_size(code);
    code->nblocks   = mu.blocks_max;
    return mu.code.v;
}

bool py_dis(Str source, Str filename, String &out)
{
    Ast ast;
    if (!ast.parse(source))
        return false;
    Root code{ py_compile(ast, filename) };
    if (code.v.is_nil())
        return false;
    return code_dis(code_of(code.v), out);
}
