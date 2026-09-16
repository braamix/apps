// Template and Interpolation, after CPython's Objects/templateobject.c and
// Objects/interpolationobject.c; see templatelib.h.
#include "templatelib.h"

#include "func.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

extern const Type template_iter_type;

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

usize tuple_len(Value v)
{
    return static_cast<TupleObj *>(v.obj())->len;
}

Value tuple_at(Value v, usize i)
{
    return static_cast<TupleObj *>(v.obj())->items()[i];
}

struct TemplateObj : Obj {
    Value strings; // TupleObj of str
    Value interps; // TupleObj of Interpolation, one fewer
};

struct InterpObj : Obj {
    Value value;
    Value expr; // str
    Value conv; // None, or 's', 'r' or 'a'
    Value spec; // str
};

// Strings and interpolations in turn, an empty string skipped.
struct TiterObj : Obj {
    Value tmpl;
    u32 si, ii;
    bool strings; // a string is next
};

TemplateObj *tmpl_of(Value v)
{
    return static_cast<TemplateObj *>(v.obj());
}

InterpObj *interp_of(Value v)
{
    return static_cast<InterpObj *>(v.obj());
}

bool is_template(Value v)
{
    return v.is_obj() && v.obj()->type == &template_type;
}

bool is_interp(Value v)
{
    return v.is_obj() && v.obj()->type == &interpolation_type;
}

void tmpl_trace(Obj *o)
{
    gc_mark(static_cast<TemplateObj *>(o)->strings);
    gc_mark(static_cast<TemplateObj *>(o)->interps);
}

void interp_trace(Obj *o)
{
    InterpObj *p = static_cast<InterpObj *>(o);
    gc_mark(p->value);
    gc_mark(p->expr);
    gc_mark(p->conv);
    gc_mark(p->spec);
}

void titer_trace(Obj *o)
{
    gc_mark(static_cast<TiterObj *>(o)->tmpl);
}

R tmpl_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append("Template(strings=") || py_repr(tmpl_of(rv.v)->strings, out) != R::Ok ||
        !out.append(", interpolations=") || py_repr(tmpl_of(rv.v)->interps, out) != R::Ok ||
        !out.push(')'))
        return err_pending() ? R::Err : oom();
    return R::Ok;
}

R interp_repr(Value v, String &out)
{
    Root rv{ v };
    if (!out.append("Interpolation("))
        return oom();
    for (u32 k = 0; k < 4; k++) {
        InterpObj *p = interp_of(rv.v);
        Root part{ k == 0 ? p->value : k == 1 ? p->expr : k == 2 ? p->conv : p->spec };
        if (k && !out.append(", "))
            return oom();
        if (py_repr(part.v, out) != R::Ok)
            return R::Err;
    }
    return out.push(')') ? R::Ok : oom();
}

Value tmpl_iter(Value v)
{
    Root rv{ v };
    TiterObj *it = static_cast<TiterObj *>(obj_alloc(&template_iter_type, sizeof(TiterObj)));
    if (!it)
        return oom(), Value();
    it->tmpl    = rv.v;
    it->si      = 0;
    it->ii      = 0;
    it->strings = true;
    return obj_value(it);
}

R titer_next(Value v, Value &out)
{
    TiterObj *it   = static_cast<TiterObj *>(v.obj());
    TemplateObj *t = tmpl_of(it->tmpl);
    if (it->strings) {
        it->strings = false;
        if (it->si >= tuple_len(t->strings))
            return R::NotImpl;
        out = tuple_at(t->strings, it->si++);
        if (str_of(out)->str().size())
            return R::Ok;
        it->strings = true;
    } else {
        it->strings = true;
    }
    if (it->ii >= tuple_len(t->interps))
        return R::NotImpl;
    out = tuple_at(t->interps, it->ii++);
    return R::Ok;
}

Value titer_self(Value v)
{
    return v;
}

// A tuple of the two runs, the last string of `a` joined to the first of `b`.
Value strings_concat(Value a, Value b)
{
    Root ra{ a }, rb{ b };
    usize na = tuple_len(ra.v), nb = tuple_len(rb.v);
    String joined;
    if (!joined.append(str_of(tuple_at(ra.v, na - 1))->str()) ||
        !joined.append(str_of(tuple_at(rb.v, 0))->str()))
        return oom(), Value();
    Root mid{ str_new(joined.str()) };
    if (mid.v.is_nil())
        return Value();
    TupleObj *t = tuple_new(na + nb - 1);
    if (!t)
        return oom(), Value();
    usize at = 0;
    for (usize k = 0; k + 1 < na; k++)
        t->items()[at++] = tuple_at(ra.v, k);
    t->items()[at++] = mid.v;
    for (usize k = 1; k < nb; k++)
        t->items()[at++] = tuple_at(rb.v, k);
    return obj_value(t);
}

Value tuple_concat(Value a, Value b)
{
    Root ra{ a }, rb{ b };
    usize na = tuple_len(ra.v), nb = tuple_len(rb.v);
    TupleObj *t = tuple_new(na + nb);
    if (!t)
        return oom(), Value();
    for (usize k = 0; k < na; k++)
        t->items()[k] = tuple_at(ra.v, k);
    for (usize k = 0; k < nb; k++)
        t->items()[na + k] = tuple_at(rb.v, k);
    return obj_value(t);
}

// Template + Template, and nothing else: a str on the left says so itself.
R tmpl_binop(Value a, Value b, Op op, Value &out)
{
    if (op != Op::Add || !is_template(a))
        return R::NotImpl;
    if (!is_template(b)) {
        Buf<160> m;
        m.put("can only concatenate string.templatelib.Template (not \"").put(type_name(b));
        m.put("\") to string.templatelib.Template");
        return err_set("TypeError", m.str());
    }
    Root ra{ a }, rb{ b };
    Root strings{ strings_concat(tmpl_of(ra.v)->strings, tmpl_of(rb.v)->strings) };
    if (strings.v.is_nil())
        return R::Err;
    Root interps{ tuple_concat(tmpl_of(ra.v)->interps, tmpl_of(rb.v)->interps) };
    if (interps.v.is_nil())
        return R::Err;
    out = template_build(strings.v, interps.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R tmpl_getattr(Value v, StrObj *name, Value &out)
{
    Str n = name->str();
    if (n == "strings")
        return out = tmpl_of(v)->strings, R::Ok;
    if (n == "interpolations")
        return out = tmpl_of(v)->interps, R::Ok;
    if (n != "values")
        return R::NotImpl;
    Root rv{ v };
    usize len   = tuple_len(tmpl_of(rv.v)->interps);
    TupleObj *t = tuple_new(len);
    if (!t)
        return oom();
    for (usize k = 0; k < len; k++)
        t->items()[k] = interp_of(tuple_at(tmpl_of(rv.v)->interps, k))->value;
    out = obj_value(t);
    return R::Ok;
}

R interp_getattr(Value v, StrObj *name, Value &out)
{
    Str n        = name->str();
    InterpObj *p = interp_of(v);
    if (n == "value")
        out = p->value;
    else if (n == "expression")
        out = p->expr;
    else if (n == "conversion")
        out = p->conv;
    else if (n == "format_spec")
        out = p->spec;
    else
        return R::NotImpl;
    return R::Ok;
}

Value to_tuple(Value list)
{
    Root rl{ list };
    TupleObj *t = tuple_new(list_of(rl.v)->items.size());
    if (!t)
        return oom(), Value();
    for (usize k = 0; k < t->len; k++)
        t->items()[k] = list_of(rl.v)->items[k];
    return obj_value(t);
}

// Template(*args): strings and Interpolations in any order, adjacent strings
// joined, and an empty string wherever two Interpolations meet.
R n_template(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "Template.__new__ only accepts *args arguments");
    for (u32 k = 0; k < a.nargs; k++)
        if (!is_str(a.args[k]) && !is_interp(a.args[k])) {
            Buf<160> m;
            m.put("Template.__new__ *args need to be of type 'str' or 'Interpolation', got ");
            m.put(type_name(a.args[k]));
            return err_set("TypeError", m.str());
        }
    ListObj *sl = list_new();
    if (!sl)
        return oom();
    Root strings{ obj_value(sl) };
    ListObj *il = list_new();
    if (!il)
        return oom();
    Root interps{ obj_value(il) };
    String run;
    for (u32 k = 0; k < a.nargs; k++) {
        if (is_str(a.args[k])) {
            if (!run.append(str_of(a.args[k])->str()))
                return oom();
            continue;
        }
        Root piece{ str_new(run.str()) };
        if (piece.v.is_nil() || !list_push(list_of(strings.v), piece.v) ||
            !list_push(list_of(interps.v), a.args[k]))
            return err_pending() ? R::Err : oom();
        run.clear();
    }
    Root tail{ str_new(run.str()) };
    if (tail.v.is_nil() || !list_push(list_of(strings.v), tail.v))
        return err_pending() ? R::Err : oom();
    Root st{ to_tuple(strings.v) };
    if (st.v.is_nil())
        return R::Err;
    Root it{ to_tuple(interps.v) };
    if (it.v.is_nil())
        return R::Err;
    out = template_build(st.v, it.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R must_be_str(Value v, Str arg)
{
    Buf<128> m;
    m.put("Interpolation() argument '").put(arg).put("' must be str, not ").put(type_name(v));
    return err_set("TypeError", m.str());
}

// Interpolation(value, expression="", conversion=None, format_spec="")
R n_interp(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "value", "expression", "conversion", "format_spec" };
    Value v[4];
    if (a.nargs > 4) {
        Buf<96> m;
        m.put("Interpolation() takes at most 4 arguments (").put(u64(a.nargs)).put(" given)");
        return err_set("TypeError", m.str());
    }
    for (u32 k = 0; k < a.nargs; k++)
        v[k] = a.args[k];
    for (u32 k = 0; k < a.nkw; k++) {
        Str key = str_of(a.kwnames[k])->str();
        u32 at  = 0;
        while (at < 4 && NAMES[at] != key)
            at++;
        if (at == 4) {
            Buf<128> m;
            m.put("Interpolation() got an unexpected keyword argument '").put(key).put("'");
            return err_set("TypeError", m.str());
        }
        if (!v[at].is_nil()) {
            Buf<128> m;
            m.put("argument for Interpolation() given by name ('")
                .put(key)
                .put("') and position (");
            m.put(u64(at + 1)).put(")");
            return err_set("TypeError", m.str());
        }
        v[at] = a.kwvals[k];
    }
    if (v[0].is_nil())
        return err_set("TypeError", "Interpolation() missing required argument 'value' (pos 1)");
    Root value{ v[0] }, expr{ v[1] }, conv{ v[2] }, spec{ v[3] };
    if (!expr.v.is_nil() && !is_str(expr.v))
        return must_be_str(expr.v, "expression");
    if (!conv.v.is_nil() && !is_none(conv.v)) {
        if (!is_str(conv.v)) {
            Buf<128> m;
            m.put("Interpolation() argument 'conversion' must be str, not ").put(type_name(conv.v));
            return err_set("TypeError", m.str());
        }
        Str c = str_of(conv.v)->str();
        if (c != Str("a") && c != Str("r") && c != Str("s"))
            return err_set("ValueError",
                           "Interpolation() argument 'conversion' must be one of 's', 'a' or 'r'");
    }
    if (!spec.v.is_nil() && !is_str(spec.v))
        return must_be_str(spec.v, "format_spec");
    if (expr.v.is_nil() && (expr = str_new(Str())).v.is_nil())
        return R::Err;
    if (spec.v.is_nil() && (spec = str_new(Str())).v.is_nil())
        return R::Err;
    InterpObj *p = static_cast<InterpObj *>(obj_alloc(&interpolation_type, sizeof(InterpObj)));
    if (!p)
        return oom();
    p->value = value.v;
    p->expr  = expr.v;
    p->conv  = conv.v.is_nil() ? value_none() : conv.v;
    p->spec  = spec.v;
    out      = obj_value(p);
    return R::Ok;
}

// `Template[int]`: a generic alias, as for a list.
R m_class_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__class_getitem__", 1, 1))
        return R::Err;
    out = genalias_new(a.args[0], a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method GENERIC_METHODS[] = {
    { "__class_getitem__", m_class_getitem },
};

} // namespace

constexpr Type template_type{ .name    = "string.templatelib.Template",
                              .trace   = tmpl_trace,
                              .repr    = tmpl_repr,
                              .binop   = tmpl_binop,
                              .iter    = tmpl_iter,
                              .getattr = tmpl_getattr };

constexpr Type interpolation_type{ .name    = "string.templatelib.Interpolation",
                                   .trace   = interp_trace,
                                   .repr    = interp_repr,
                                   .getattr = interp_getattr };

constexpr Type template_iter_type{ .name  = "string.templatelib.TemplateIter",
                                   .trace = titer_trace,
                                   .iter  = titer_self,
                                   .next  = titer_next };

Value template_build(Value strings, Value interps)
{
    Root rs{ strings }, ri{ interps };
    TemplateObj *t = static_cast<TemplateObj *>(obj_alloc(&template_type, sizeof(TemplateObj)));
    if (!t)
        return oom(), Value();
    t->strings = rs.v;
    t->interps = ri.v;
    return obj_value(t);
}

Value interp_build(Value value, Value expr, u32 conv, Value spec)
{
    Root rv{ value }, re{ expr }, rs{ spec };
    Root rc{ value_none() };
    if (conv) {
        char c[1] = { char(conv) };
        rc        = str_new(Str(c, 1));
        if (rc.v.is_nil())
            return Value();
    }
    if (rs.v.is_nil() && (rs = str_new(Str())).v.is_nil())
        return Value();
    InterpObj *p = static_cast<InterpObj *>(obj_alloc(&interpolation_type, sizeof(InterpObj)));
    if (!p)
        return oom(), Value();
    p->value = rv.v;
    p->expr  = re.v;
    p->conv  = rc.v;
    p->spec  = rs.v;
    return obj_value(p);
}

bool templatelib_methods()
{
    if (!method_install(&template_type, GENERIC_METHODS) ||
        !method_install(&interpolation_type, GENERIC_METHODS))
        return false;
    Root tfn{ native_new("Template", n_template) };
    Root ifn{ native_new("Interpolation", n_interp) };
    if (tfn.v.is_nil() || ifn.v.is_nil() || !type_set_ctor(&template_type, tfn.v) ||
        !type_set_ctor(&interpolation_type, ifn.v))
        return false;
    Root cls{ type_wrap(&interpolation_type) };
    TupleObj *t = tuple_new(4);
    if (cls.v.is_nil() || !t)
        return false;
    Root names{ obj_value(t) };
    constexpr Str FIELDS[] = { "value", "expression", "conversion", "format_spec" };
    for (u32 k = 0; k < 4; k++) {
        Value s = str_new(FIELDS[k]);
        if (s.is_nil())
            return false;
        static_cast<TupleObj *>(names.v.obj())->items()[k] = s;
    }
    return mod_put(static_cast<DictObj *>(type_obj(cls.v)->dict.obj()), "__match_args__", names.v);
}
