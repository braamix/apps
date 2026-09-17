// FrameLocalsProxy (PEP 667): a function frame's f_locals, read and written
// through to its slots and cells.
//
// A name that is a local is the slot or the cell; any other key lives in the
// frame's `extra` dict. A local cannot be deleted. Each read of f_locals makes
// a new proxy, as CPython's does.
#include "exc.h"
#include "frame.h"
#include "gc.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct FlpObj : Obj {
    Value frame;
};

void flp_trace(Obj *o)
{
    gc_mark(static_cast<FlpObj *>(o)->frame);
}

FrameObj *frame_at(Value proxy)
{
    return frame_of(static_cast<FlpObj *>(proxy.obj())->frame);
}

bool is_flp(Value v)
{
    return is_frame_locals(v);
}

CellObj *cell_at(FrameObj *f, usize i)
{
    Value c = static_cast<TupleObj *>(f->cells.obj())->items()[i];
    return c.is_nil() ? nullptr : static_cast<CellObj *>(c.obj());
}

usize find(const Vec<Value> &names, Str k)
{
    for (usize i = 0; i < names.size(); i++)
        if (str_of(names[i])->str() == k)
            return i;
    return Str::npos;
}

// Where the local `key` lives: a slot, a cell, or nowhere. A parameter that a
// nested scope captures is its cell.
struct Place {
    Value *slot   = nullptr;
    CellObj *cell = nullptr;
    bool local    = false;

    Value get() const { return cell ? cell->v : slot ? *slot : Value(); }
};

Place locate(FrameObj *f, Value key)
{
    Place p;
    if (!is_str(key))
        return p;
    CodeObj *c = code_of(f->code);
    Str k      = str_of(key)->str();
    usize own  = c->cellvars.size();
    usize i    = find(c->cellvars, k);
    usize j    = i == Str::npos ? find(c->freevars, k) : Str::npos;
    if (i != Str::npos || j != Str::npos) {
        p.local = true;
        if (!f->cells.is_nil())
            p.cell = cell_at(f, i != Str::npos ? i : own + j);
        if (p.cell || j != Str::npos)
            return p;
    }
    i = find(c->varnames, k);
    if (i != Str::npos) {
        p.local = true;
        p.slot  = &f->slots()[i];
    }
    return p;
}

DictObj *extra_of(FrameObj *f)
{
    return f->extra.is_nil() ? nullptr : static_cast<DictObj *>(f->extra.obj());
}

// Every bound local in order, then the extra keys: what dict(proxy) is.
Value snapshot(Value proxy)
{
    Root rp{ proxy };
    DictObj *d = dict_new();
    if (!d)
        return oom(), Value();
    Root rd{ obj_value(d) };
    auto put = [&](Value k, Value v) {
        return v.is_nil() || dict_set(static_cast<DictObj *>(rd.v.obj()), k, v) == R::Ok;
    };
    FrameObj *f = frame_at(rp.v);
    CodeObj *c  = code_of(f->code);
    for (usize i = 0; i < c->varnames.size(); i++)
        if (!put(c->varnames[i], locate(f, c->varnames[i]).get()))
            return Value();
    if (!f->cells.is_nil()) {
        usize own = c->cellvars.size();
        for (usize i = 0; i < own + c->freevars.size(); i++) {
            Value name = i < own ? c->cellvars[i] : c->freevars[i - own];
            if (i < own && find(c->varnames, str_of(name)->str()) != Str::npos)
                continue;
            CellObj *cell = cell_at(frame_at(rp.v), i);
            if (cell && !put(name, cell->v))
                return Value();
        }
    }
    if (DictObj *x = extra_of(frame_at(rp.v))) {
        usize at = 0;
        Value k, v;
        while (table_next(x->t, at, k, v))
            if (!put(k, v))
                return Value();
    }
    return rd.v;
}

// What a mapping that is not a proxy stands for; the snapshot of one that is.
Value as_dict(Value v)
{
    if (is_flp(v))
        return snapshot(v);
    return is_dict(v) ? v : Value();
}

R hashable(Value key)
{
    u32 h = 0;
    return py_hash(key, h);
}

R not_defined(Value key)
{
    Root rk{ key };
    String s;
    if (!s.append("local variable '") || (py_repr(rk.v, s) != R::Ok) ||
        !s.append("' is not defined"))
        return err_pending() ? R::Err : oom();
    Value m = str_new(s.str());
    if (m.is_nil())
        return R::Err;
    return key_error(m);
}

R no_removal()
{
    return err_set("ValueError", "cannot remove local variables from FrameLocalsProxy");
}

// NotImpl when the key is neither a bound local nor an extra.
R lookup(Value proxy, Value key, Value &out)
{
    if (hashable(key) != R::Ok)
        return R::Err;
    FrameObj *f = frame_at(proxy);
    Place p     = locate(f, key);
    if (p.local) {
        out = p.get();
        return out.is_nil() ? R::NotImpl : R::Ok;
    }
    DictObj *x = extra_of(f);
    return x ? dict_get(x, key, out) : R::NotImpl;
}

R store(Value proxy, Value key, Value v)
{
    if (hashable(key) != R::Ok)
        return R::Err;
    FrameObj *f = frame_at(proxy);
    Place p     = locate(f, key);
    if (p.cell) {
        p.cell->v = v;
        return R::Ok;
    }
    if (p.slot) {
        *p.slot = v;
        return R::Ok;
    }
    Root rp{ proxy }, rk{ key }, rv{ v };
    if (!extra_of(f)) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        frame_at(rp.v)->extra = obj_value(d);
    }
    return dict_set(extra_of(frame_at(rp.v)), rk.v, rv.v);
}

// NotImpl when there is nothing to remove.
R remove(Value proxy, Value key)
{
    if (hashable(key) != R::Ok)
        return R::Err;
    FrameObj *f = frame_at(proxy);
    if (locate(f, key).local)
        return no_removal();
    DictObj *x = extra_of(f);
    return x ? dict_del(x, key) : R::NotImpl;
}

R flp_getitem(Value v, Value key, Value &out)
{
    R r = lookup(v, key, out);
    return r == R::NotImpl ? not_defined(key) : r;
}

R flp_setitem(Value v, Value key, Value x)
{
    return store(v, key, x);
}

R flp_delitem(Value v, Value key)
{
    R r = remove(v, key);
    return r == R::NotImpl ? key_error(key) : r;
}

R flp_contains(Value v, Value key, bool &out)
{
    Value got;
    R r = lookup(v, key, got);
    out = r == R::Ok;
    return r == R::Err ? R::Err : R::Ok;
}

R flp_len(Value v, usize &out)
{
    Value d = snapshot(v);
    if (d.is_nil())
        return R::Err;
    out = static_cast<DictObj *>(d.obj())->t.live;
    return R::Ok;
}

Value flp_iter(Value v)
{
    Root d{ snapshot(v) };
    if (d.v.is_nil())
        return Value();
    ListObj *keys = py_list_of(d.v);
    return keys ? py_iter(obj_value(keys)) : Value();
}

R flp_repr(Value v, String &out)
{
    if (!repr_enter(v))
        return out.append("{...}") ? R::Ok : oom();
    Root d{ snapshot(v) };
    R r = d.v.is_nil() ? R::Err : py_repr(d.v, out);
    repr_leave();
    return r;
}

R flp_eq(Value a, Value b, bool &out)
{
    if (!is_flp(a)) {
        Value t = a;
        a       = b;
        b       = t;
    }
    Root rb{ b };
    Root x{ snapshot(a) };
    if (x.v.is_nil())
        return R::Err;
    Root y{ as_dict(rb.v) };
    if (y.v.is_nil())
        return err_pending() ? R::Err : R::NotImpl;
    return py_eq(x.v, y.v, out);
}

// `proxy | mapping` and `mapping | proxy` are dicts.
R flp_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Or)
        return R::NotImpl;
    Root ra{ a }, rb{ b };
    Root x{ as_dict(ra.v) };
    if (x.v.is_nil())
        return err_pending() ? R::Err : R::NotImpl;
    Root y{ as_dict(rb.v) };
    if (y.v.is_nil())
        return err_pending() ? R::Err : R::NotImpl;
    DictObj *d = dict_new();
    if (!d)
        return oom();
    Root rd{ obj_value(d) };
    Value both[2] = { x.v, y.v };
    for (Value from : both) {
        usize at = 0;
        Value k, v;
        while (table_next(static_cast<DictObj *>(from.obj())->t, at, k, v))
            if (dict_set(static_cast<DictObj *>(rd.v.obj()), k, v) != R::Ok)
                return R::Err;
    }
    out = rd.v;
    return R::Ok;
}

// ------------------------------------------------------------------ methods

Value self_of(const CallArgs &a)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_flp(s))
        return err_set("TypeError", "FrameLocalsProxy method needs a FrameLocalsProxy"), Value();
    return s;
}

bool no_kw(const CallArgs &a, Str who)
{
    if (!a.nkw)
        return true;
    Buf<96> b;
    b.put("FrameLocalsProxy.").put(who).put("() takes no keyword arguments");
    return err_set("TypeError", b.str()) == R::Ok;
}

// get, pop and setdefault count their arguments CPython's way.
bool arity(const CallArgs &a, Str who, bool at_least)
{
    u32 n = a.nargs - 1;
    if (n >= 1 && n <= 2)
        return true;
    char t[24];
    Buf<96> b;
    b.put(who);
    if (!at_least)
        b.put(" expected 1 or 2 arguments");
    else if (n < 1)
        b.put(" expected at least 1 argument, got ").put(int_text(t, sizeof t, i64(n)));
    else
        b.put(" expected at most 2 arguments, got ").put(int_text(t, sizeof t, i64(n)));
    return err_set("TypeError", b.str()) == R::Ok;
}

R m_get(const CallArgs &a, Value &out)
{
    Value s = self_of(a);
    if (s.is_nil() || !no_kw(a, "get") || !arity(a, "get", false))
        return R::Err;
    R r = lookup(s, a.args[1], out);
    if (r == R::NotImpl)
        out = a.nargs > 2 ? a.args[2] : value_none();
    return r == R::Err ? R::Err : R::Ok;
}

R m_pop(const CallArgs &a, Value &out)
{
    Value s = self_of(a);
    if (s.is_nil() || !no_kw(a, "pop") || !arity(a, "pop", true))
        return R::Err;
    Root rs{ s };
    if (locate(frame_at(rs.v), a.args[1]).local)
        return hashable(a.args[1]) != R::Ok ? R::Err : no_removal();
    R r = lookup(rs.v, a.args[1], out);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl) {
        if (a.nargs > 2) {
            out = a.args[2];
            return R::Ok;
        }
        return key_error(a.args[1]);
    }
    Root got{ out };
    if (remove(rs.v, a.args[1]) == R::Err)
        return R::Err;
    out = got.v;
    return R::Ok;
}

R m_setdefault(const CallArgs &a, Value &out)
{
    Value s = self_of(a);
    if (s.is_nil() || !no_kw(a, "setdefault") || !arity(a, "setdefault", false))
        return R::Err;
    Root rs{ s };
    R r = lookup(rs.v, a.args[1], out);
    if (r != R::NotImpl)
        return r;
    out = a.nargs > 2 ? a.args[2] : value_none();
    return store(rs.v, a.args[1], out);
}

R m_update(const CallArgs &a, Value &out)
{
    Value s = self_of(a);
    if (s.is_nil() || !no_kw(a, "update") || !meth_args(a, "update", 1, 1))
        return R::Err;
    Root rs{ s };
    Root from{ as_dict(a.args[1]) };
    if (from.v.is_nil())
        return err_pending()
                   ? R::Err
                   : err_set("TypeError",
                             "update() argument must be dict or another FrameLocalsProxy");
    usize at = 0;
    Value k, v;
    while (table_next(static_cast<DictObj *>(from.v.obj())->t, at, k, v))
        if (store(rs.v, k, v) != R::Ok)
            return R::Err;
    out = value_none();
    return R::Ok;
}

R m_copy(const CallArgs &a, Value &out)
{
    Value s = self_of(a);
    if (s.is_nil() || !no_kw(a, "copy"))
        return R::Err;
    if (a.nargs > 1) {
        char t[24];
        Buf<96> b;
        b.put("FrameLocalsProxy.copy() takes no arguments (");
        b.put(int_text(t, sizeof t, i64(a.nargs - 1))).put(" given)");
        return err_set("TypeError", b.str());
    }
    out = snapshot(s);
    return out.is_nil() ? R::Err : R::Ok;
}

// keys(), values(), items() and reversed() are lists, as CPython's are.
R listed(const CallArgs &a, Value &out, Str who, u32 kind)
{
    Value s = self_of(a);
    if (s.is_nil() || !meth_args(a, who, 0, 0))
        return R::Err;
    Root d{ snapshot(s) };
    if (d.v.is_nil())
        return R::Err;
    ListObj *l = nullptr;
    if (kind == 3) {
        l = py_list_of(d.v);
        if (l) {
            Vec<Value> &xs = l->items;
            for (usize i = 0, j = xs.size(); i + 1 < j; i++, j--) {
                Value t   = xs[i];
                xs[i]     = xs[j - 1];
                xs[j - 1] = t;
            }
        }
    } else {
        Root view{ dict_view(d.v, kind) };
        if (view.v.is_nil())
            return R::Err;
        l = py_list_of(view.v);
    }
    out = obj_value(l);
    return l ? R::Ok : R::Err;
}

R m_keys(const CallArgs &a, Value &out)
{
    return listed(a, out, "keys", VIEW_KEYS);
}

R m_values(const CallArgs &a, Value &out)
{
    return listed(a, out, "values", VIEW_VALUES);
}

R m_items(const CallArgs &a, Value &out)
{
    return listed(a, out, "items", VIEW_ITEMS);
}

R m_reversed(const CallArgs &a, Value &out)
{
    return listed(a, out, "__reversed__", 3);
}

constexpr Method FLP_METHODS[] = {
    { "get", m_get },       { "pop", m_pop },     { "setdefault", m_setdefault },
    { "update", m_update }, { "copy", m_copy },   { "keys", m_keys },
    { "values", m_values }, { "items", m_items }, { "__reversed__", m_reversed },
};

} // namespace

constexpr Type frame_locals_type{ .name     = "FrameLocalsProxy",
                                  .trace    = flp_trace,
                                  .eq       = flp_eq,
                                  .repr     = flp_repr,
                                  .len      = flp_len,
                                  .getitem  = flp_getitem,
                                  .setitem  = flp_setitem,
                                  .delitem  = flp_delitem,
                                  .contains = flp_contains,
                                  .binop    = flp_binop,
                                  .iter     = flp_iter,
                                  .patma    = PATMA_MAP,
                                  .final    = true };

namespace {

R b_flp(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "FrameLocalsProxy takes no keyword arguments");
    if (a.nargs != 1) {
        char t[24];
        Buf<96> b;
        b.put("FrameLocalsProxy expected 1 argument, got ")
            .put(int_text(t, sizeof t, i64(a.nargs)));
        return err_set("TypeError", b.str());
    }
    if (!is_frame(a.args[0])) {
        Buf<96> b;
        b.put("expect frame, not ").put(type_name(a.args[0]));
        return err_set("TypeError", b.str());
    }
    out = frame_locals_proxy(a.args[0]);
    return out.is_nil() ? R::Err : R::Ok;
}

} // namespace

Value frame_locals_proxy(Value frame)
{
    Root rf{ frame };
    FlpObj *p = static_cast<FlpObj *>(obj_alloc(&frame_locals_type, sizeof(FlpObj)));
    if (!p)
        return oom(), Value();
    p->frame = rf.v;
    return obj_value(p);
}

Value frame_locals_dict(Value v)
{
    return is_flp(v) ? snapshot(v) : v;
}

bool frame_locals_methods()
{
    if (!method_install(&frame_locals_type, FLP_METHODS))
        return false;
    Root fn{ native_new("FrameLocalsProxy", b_flp) };
    return !fn.v.is_nil() && type_set_ctor(&frame_locals_type, fn.v);
}
