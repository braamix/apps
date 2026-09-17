// `_contextvars`: ContextVar, Token, Context and copy_context (PEP 567).
//
// CPython keeps a context's variables in an immutable map, so a copy is free.
// Here a context owns a dict and a copy copies it; with one thread, the only
// current context is the process's own, or the one Context.run entered.
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/alloc.h"
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

extern const Type var_type;
extern const Type token_type;
extern const Type context_type;
extern const Type missing_type;

struct VarObj : Obj {
    Value name;
    Value dflt; // Nil when there is none
};

struct TokenObj : Obj {
    Value ctx;
    Value var;
    Value old; // Nil for Token.MISSING
    bool used;
};

struct ContextObj : Obj {
    Value vars; // DictObj: var to value
    bool entered;
};

VarObj *var_of(Value v)
{
    return static_cast<VarObj *>(v.obj());
}

TokenObj *token_of(Value v)
{
    return static_cast<TokenObj *>(v.obj());
}

ContextObj *ctx_of(Value v)
{
    return static_cast<ContextObj *>(v.obj());
}

DictObj *vars_of(Value ctx)
{
    return static_cast<DictObj *>(ctx_of(ctx)->vars.obj());
}

bool is_var(Value v)
{
    return v.is_obj() && v.obj()->type == &var_type;
}

// The current context.
struct Home {
    Value current;
};

Home *home;

void home_mark()
{
    gc_mark(home->current);
}

Obj missing_obj{ &missing_type, nullptr, nullptr, OBJ_IMMORTAL };

R missing_repr(Value, String &out)
{
    return out.append("<Token.MISSING>") ? R::Ok : oom();
}

constexpr Type missing_type{ .name = "Token.MISSING", .repr = missing_repr, .final = true };

Value context_make(Value from)
{
    Root rf{ from };
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    if (!rf.v.is_nil()) {
        usize at = 0;
        Value k, x;
        while (table_next(vars_of(rf.v)->t, at, k, x))
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), k, x) != R::Ok)
                return Value();
    }
    ContextObj *c = static_cast<ContextObj *>(obj_alloc(&context_type, sizeof(ContextObj)));
    if (!c)
        return oom(), Value();
    c->vars    = rd.v;
    c->entered = false;
    return obj_value(c);
}

Value current()
{
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return oom(), Value();
        gc_root_hook(home_mark);
    }
    if (home->current.is_nil()) {
        Value c = context_make(Value());
        if (c.is_nil())
            return Value();
        home->current = c;
    }
    return home->current;
}

R need_var(Value key)
{
    if (is_var(key))
        return R::Ok;
    Root rk{ key };
    String s;
    if (!s.append("a ContextVar key was expected, got "))
        return oom();
    if (py_repr(rk.v, s) != R::Ok)
        return R::Err;
    return err_set("TypeError", s.str());
}

// ------------------------------------------------------------------ ContextVar

void var_trace(Obj *o)
{
    VarObj *v = static_cast<VarObj *>(o);
    gc_mark(v->name);
    gc_mark(v->dflt);
}

R var_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append("<ContextVar name="))
        return oom();
    if (py_repr(var_of(rv.v)->name, out) != R::Ok)
        return R::Err;
    if (!var_of(rv.v)->dflt.is_nil()) {
        if (!out.append(" default="))
            return oom();
        if (py_repr(var_of(rv.v)->dflt, out) != R::Ok)
            return R::Err;
    }
    char tmp[24];
    if (!out.append(" at ") || !out.append(addr_text(tmp, sizeof tmp, rv.v.obj())) ||
        !out.push('>'))
        return oom();
    return R::Ok;
}

R var_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != "name")
        return R::NotImpl;
    out = var_of(v)->name;
    return R::Ok;
}

VarObj *self_var(const CallArgs &a, Str who)
{
    Value s = a.nargs ? a.args[0] : Value();
    if (!is_var(s)) {
        Buf<128> b;
        b.put("descriptor '")
            .put(who)
            .put("' for '_contextvars.ContextVar' objects doesn't apply to a '");
        b.put(type_name(s)).put("' object");
        return err_set("TypeError", b.str()), nullptr;
    }
    return var_of(s);
}

R v_get(const CallArgs &a, Value &out)
{
    VarObj *v = self_var(a, "get");
    if (!v)
        return R::Err;
    if (a.nkw)
        return err_set("TypeError", "get() takes no keyword arguments");
    if (a.nargs > 2) {
        char tmp[24];
        Buf<96> b;
        b.put("get expected at most 1 argument, got ").put(int_text(tmp, sizeof tmp, a.nargs - 1));
        return err_set("TypeError", b.str());
    }
    Root self{ a.args[0] };
    Value ctx = current();
    if (ctx.is_nil())
        return R::Err;
    R r = dict_get(vars_of(ctx), self.v, out);
    if (r != R::NotImpl)
        return r;
    if (a.nargs == 2) {
        out = a.args[1];
        return R::Ok;
    }
    if (!var_of(self.v)->dflt.is_nil()) {
        out = var_of(self.v)->dflt;
        return R::Ok;
    }
    TupleObj *t = tuple_new(1);
    if (!t)
        return oom();
    t->items()[0] = self.v;
    Root rt{ obj_value(t) };
    Value e = exc_new(exc_find("LookupError"), rt.v);
    if (e.is_nil())
        return R::Err;
    return err_set_value(e, "LookupError");
}

R v_set(const CallArgs &a, Value &out)
{
    VarObj *v = self_var(a, "set");
    if (!v)
        return R::Err;
    if (a.nkw)
        return err_set("TypeError", "ContextVar.set() takes no keyword arguments");
    if (a.nargs != 2) {
        char tmp[24];
        Buf<96> b;
        b.put("ContextVar.set() takes exactly one argument (");
        b.put(int_text(tmp, sizeof tmp, a.nargs - 1)).put(" given)");
        return err_set("TypeError", b.str());
    }
    Root self{ a.args[0] }, val{ a.args[1] };
    Root ctx{ current() };
    if (ctx.v.is_nil())
        return R::Err;
    Root old;
    R r = dict_get(vars_of(ctx.v), self.v, old.v);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl)
        old.v = Value();
    TokenObj *t = static_cast<TokenObj *>(obj_alloc(&token_type, sizeof(TokenObj)));
    if (!t)
        return oom();
    t->ctx  = ctx.v;
    t->var  = self.v;
    t->old  = old.v;
    t->used = false;
    Root rt{ obj_value(t) };
    if (dict_set(vars_of(ctx.v), self.v, val.v) != R::Ok)
        return R::Err;
    out = rt.v;
    return R::Ok;
}

R v_reset(const CallArgs &a, Value &out)
{
    VarObj *v = self_var(a, "reset");
    if (!v || !meth_args(a, "reset", 1, 1))
        return R::Err;
    Value tk = a.args[1];
    if (!tk.is_obj() || tk.obj()->type != &token_type) {
        Root rk{ tk };
        String s;
        if (!s.append("expected an instance of Token, got "))
            return oom();
        if (py_repr(rk.v, s) != R::Ok)
            return R::Err;
        return err_set("TypeError", s.str());
    }
    TokenObj *t = token_of(tk);
    Root rt{ tk };
    auto token_says = [&](Str kind, Str what) {
        String s;
        if (py_repr(rt.v, s) != R::Ok)
            return R::Err;
        if (!s.append(what))
            return oom();
        return err_set(kind, s.str());
    };
    if (t->used)
        return token_says("RuntimeError", " has already been used once");
    if (t->var.obj() != a.args[0].obj())
        return token_says("ValueError", " was created by a different ContextVar");
    Value ctx = current();
    if (ctx.is_nil())
        return R::Err;
    if (t->ctx.obj() != ctx.obj())
        return token_says("ValueError", " was created in a different Context");
    t->used = true;
    Root self{ a.args[0] };
    if (t->old.is_nil()) {
        if (dict_del(vars_of(ctx), self.v) == R::Err)
            return R::Err;
    } else if (dict_set(vars_of(ctx), self.v, token_of(rt.v)->old) != R::Ok) {
        return R::Err;
    }
    out = value_none();
    return R::Ok;
}

constexpr Method VAR_METHODS[] = {
    { "get", v_get },
    { "set", v_set },
    { "reset", v_reset },
};

constexpr Type var_type{ .name    = "_contextvars.ContextVar",
                         .trace   = var_trace,
                         .repr    = var_repr,
                         .getattr = var_getattr,
                         .final   = true };

R b_var(const CallArgs &a, Value &out)
{
    Value name, dflt;
    if (a.nargs > 1) {
        char tmp[24];
        Buf<96> b;
        b.put("ContextVar() takes at most 1 positional argument (");
        b.put(int_text(tmp, sizeof tmp, a.nargs)).put(" given)");
        return err_set("TypeError", b.str());
    }
    if (a.nargs)
        name = a.args[0];
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n == "name" && name.is_nil()) {
            name = a.kwvals[k];
        } else if (n == "default") {
            dflt = a.kwvals[k];
        } else {
            Buf<128> b;
            b.put("ContextVar() got an unexpected keyword argument '").put(n).put('\'');
            return err_set("TypeError", b.str());
        }
    }
    if (name.is_nil())
        return err_set("TypeError", "ContextVar() takes exactly 1 positional argument (0 given)");
    if (!is_str(name))
        return err_set("TypeError", "context variable name must be a str");
    Root rn{ name }, rd{ dflt };
    VarObj *v = static_cast<VarObj *>(obj_alloc(&var_type, sizeof(VarObj)));
    if (!v)
        return oom();
    v->name = rn.v;
    v->dflt = rd.v;
    out     = obj_value(v);
    return R::Ok;
}

// ------------------------------------------------------------------ Token

void token_trace(Obj *o)
{
    TokenObj *t = static_cast<TokenObj *>(o);
    gc_mark(t->ctx);
    gc_mark(t->var);
    gc_mark(t->old);
}

R token_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append(token_of(rv.v)->used ? "<Token used var=" : "<Token var="))
        return oom();
    if (py_repr(token_of(rv.v)->var, out) != R::Ok)
        return R::Err;
    char tmp[24];
    if (!out.append(" at ") || !out.append(addr_text(tmp, sizeof tmp, rv.v.obj())) ||
        !out.push('>'))
        return oom();
    return R::Ok;
}

R token_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "var")
        out = token_of(v)->var;
    else if (n == "old_value")
        out = token_of(v)->old.is_nil() ? Value::of_obj(&missing_obj) : token_of(v)->old;
    else if (n == "MISSING")
        out = Value::of_obj(&missing_obj);
    else
        return R::NotImpl;
    return R::Ok;
}

R t_enter(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__enter__", 0, 0))
        return R::Err;
    out = a.args[0];
    return R::Ok;
}

// __exit__ is the token's var reset with it.
R t_exit(const CallArgs &a, Value &out)
{
    if (!a.nargs || !a.args[0].is_obj() || a.args[0].obj()->type != &token_type)
        return err_set("TypeError", "__exit__() requires a Token");
    Value args[2] = { token_of(a.args[0])->var, a.args[0] };
    CallArgs r;
    r.args  = args;
    r.nargs = 2;
    return v_reset(r, out);
}

constexpr Method TOKEN_METHODS[] = {
    { "__enter__", t_enter },
    { "__exit__", t_exit },
};

constexpr Type token_type{ .name    = "_contextvars.Token",
                           .trace   = token_trace,
                           .repr    = token_repr,
                           .getattr = token_getattr,
                           .final   = true };

R b_token(const CallArgs &, Value &)
{
    return err_set("RuntimeError", "Tokens can only be created by ContextVars");
}

// ------------------------------------------------------------------ Context

void context_trace(Obj *o)
{
    gc_mark(static_cast<ContextObj *>(o)->vars);
}

R context_repr(Value v, String &out)
{
    char tmp[24];
    Buf<96> b;
    b.put("<_contextvars.Context object at ").put(addr_text(tmp, sizeof tmp, v.obj())).put('>');
    return out.append(b.str()) ? R::Ok : oom();
}

R context_len(Value v, usize &out)
{
    out = vars_of(v)->t.live;
    return R::Ok;
}

R context_getitem(Value v, Value key, Value &out)
{
    if (need_var(key) != R::Ok)
        return R::Err;
    R r = dict_get(vars_of(v), key, out);
    return r == R::NotImpl ? key_error(key) : r;
}

R context_contains(Value v, Value key, bool &out)
{
    if (need_var(key) != R::Ok)
        return R::Err;
    Value got;
    R r = dict_get(vars_of(v), key, got);
    out = r == R::Ok;
    return r == R::Err ? R::Err : R::Ok;
}

Value context_iter(Value v)
{
    return py_iter(ctx_of(v)->vars);
}

R context_eq(Value a, Value b, bool &out)
{
    if (!b.is_obj() || b.obj()->type != &context_type)
        return R::NotImpl;
    return py_eq(ctx_of(a)->vars, ctx_of(b)->vars, out);
}

ContextObj *self_ctx(const CallArgs &a, Str who)
{
    Value s = a.nargs ? a.args[0] : Value();
    if (!s.is_obj() || s.obj()->type != &context_type) {
        Buf<128> b;
        b.put("descriptor '")
            .put(who)
            .put("' for '_contextvars.Context' objects doesn't apply to a '");
        b.put(type_name(s)).put("' object");
        return err_set("TypeError", b.str()), nullptr;
    }
    return ctx_of(s);
}

R c_get(const CallArgs &a, Value &out)
{
    if (!self_ctx(a, "get") || !meth_args(a, "get", 1, 2))
        return R::Err;
    if (need_var(a.args[1]) != R::Ok)
        return R::Err;
    R r = dict_get(vars_of(a.args[0]), a.args[1], out);
    if (r == R::NotImpl)
        out = a.nargs > 2 ? a.args[2] : value_none();
    return r == R::Err ? R::Err : R::Ok;
}

// keys(), values() and items() are the dict's own views.
R c_view(const CallArgs &a, Value &out, Str who, u32 kind)
{
    if (!self_ctx(a, who) || !meth_args(a, who, 0, 0))
        return R::Err;
    out = dict_view(ctx_of(a.args[0])->vars, kind);
    return out.is_nil() ? R::Err : R::Ok;
}

R c_keys(const CallArgs &a, Value &out)
{
    return c_view(a, out, "keys", VIEW_KEYS);
}

R c_values(const CallArgs &a, Value &out)
{
    return c_view(a, out, "values", VIEW_VALUES);
}

R c_items(const CallArgs &a, Value &out)
{
    return c_view(a, out, "items", VIEW_ITEMS);
}

R c_copy(const CallArgs &a, Value &out)
{
    if (!self_ctx(a, "copy") || !meth_args(a, "copy", 0, 0))
        return R::Err;
    out = context_make(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

// run(callable, *args, **kwargs): s[0] the context, s[1] the one it replaced.
void run_leave(ContObj *k)
{
    ctx_of(k->s[0])->entered = false;
    home->current            = k->s[1];
}

R run_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        k->catching = CATCH_ANY;
        return cont_call_kw(k, k->s[2], k->s[3], k->s[4], k->s[5]);
    }
    run_leave(k);
    k->fail     = nullptr;
    k->catching = CATCH_NONE;
    if (!k->caught.is_nil()) {
        Value c   = k->caught;
        k->caught = Value();
        return err_set_value(c);
    }
    return cont_done(k, in);
}

R c_run(const CallArgs &a, Value &out)
{
    if (!self_ctx(a, "run"))
        return R::Err;
    if (a.nargs < 2)
        return err_set("TypeError", "run() missing 1 required positional argument");
    Root self{ a.args[0] };
    if (ctx_of(self.v)->entered) {
        String s;
        if (!s.append("cannot enter context: "))
            return oom();
        if (py_repr(self.v, s) != R::Ok)
            return R::Err;
        if (!s.append(" is already entered"))
            return oom();
        return err_set("RuntimeError", s.str());
    }
    Root prev{ current() };
    if (prev.v.is_nil())
        return R::Err;
    TupleObj *args = tuple_new(a.nargs - 2);
    if (!args)
        return oom();
    for (u32 i = 2; i < a.nargs; i++)
        args->items()[i - 2] = a.args[i];
    Root ra{ obj_value(args) };
    Root names, vals;
    if (a.nkw) {
        TupleObj *n = tuple_new(a.nkw);
        if (!n)
            return oom();
        for (u32 i = 0; i < a.nkw; i++)
            n->items()[i] = a.kwnames[i];
        names.v     = obj_value(n);
        TupleObj *v = tuple_new(a.nkw);
        if (!v)
            return oom();
        for (u32 i = 0; i < a.nkw; i++)
            v->items()[i] = a.kwvals[i];
        vals.v = obj_value(v);
    }
    Root kv{ cont_new(run_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k              = cont_of(kv.v);
    k->s[0]                 = self.v;
    k->s[1]                 = prev.v;
    k->s[2]                 = a.args[1];
    k->s[3]                 = ra.v;
    k->s[4]                 = names.v;
    k->s[5]                 = vals.v;
    k->fail                 = run_leave;
    ctx_of(self.v)->entered = true;
    home->current           = self.v;
    out                     = kv.v;
    return R::Ok;
}

constexpr Method CONTEXT_METHODS[] = {
    { "get", c_get },     { "keys", c_keys }, { "values", c_values },
    { "items", c_items }, { "copy", c_copy }, { "run", c_run },
};

constexpr Type context_type{ .name     = "_contextvars.Context",
                             .trace    = context_trace,
                             .eq       = context_eq,
                             .repr     = context_repr,
                             .len      = context_len,
                             .getitem  = context_getitem,
                             .contains = context_contains,
                             .iter     = context_iter,
                             .final    = true };

R b_context(const CallArgs &a, Value &out)
{
    if (a.nargs || a.nkw)
        return err_set("TypeError", "Context() does not accept any arguments");
    out = context_make(Value());
    return out.is_nil() ? R::Err : R::Ok;
}

R b_copy_context(const CallArgs &a, Value &out)
{
    if (!args_only(a, "copy_context", 0, 0))
        return R::Err;
    Value c = current();
    if (c.is_nil())
        return R::Err;
    out = context_make(c);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef CTX_DEFS[] = {
    { "copy_context", b_copy_context },
};

} // namespace

bool contextvars_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (current().is_nil())
        return false;
    if (!method_install(&var_type, VAR_METHODS) || !method_install(&token_type, TOKEN_METHODS) ||
        !method_install(&context_type, CONTEXT_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_type(d, &var_type, b_var) || !mod_type(d, &token_type, b_token) ||
        !mod_type(d, &context_type, b_context) || !mod_defs(d, CTX_DEFS))
        return false;
    static const Type *const GENERIC[] = { &var_type };
    Root tt{ type_wrap(&token_type) };
    if (tt.v.is_nil() || !genalias_install(GENERIC, 1))
        return false;
    StrObj *k2 = str_intern("MISSING");
    return k2 && dict_set(static_cast<DictObj *>(type_obj(tt.v)->dict.obj()), obj_value(k2),
                          Value::of_obj(&missing_obj)) == R::Ok;
}
