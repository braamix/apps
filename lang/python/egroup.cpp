// Exception groups, after CPython's Objects/exceptions.c.
//
// A split walks a tree of groups, and two of its steps may be Python: the
// condition, when it is a callable, and `derive`, which a subclass may write.
// So the walk is a continuation over an explicit stack rather than a
// recursive function, which ground rule 2 would not allow anyway. The same
// walk, matching by identity, is what `except*` uses to work out which of the
// exceptions its clauses raised were the ones it caught.
//
// CPython tells a re-raised exception from a new one by comparing the
// traceback, the cause, the context and the notes with the original's. There
// is no traceback object here, so the other three decide.
#include "egroup.h"

#include "call.h"
#include "func.h"
#include "gc.h"
#include "genalias.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "ops.h"
#include "type.h"

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

ExcObj *exc_of(Value v)
{
    return static_cast<ExcObj *>(v.obj());
}

const ExcType *beg()
{
    return exc_find("BaseExceptionGroup");
}

Value list_tuple(Value list)
{
    Root rl{ list };
    TupleObj *t = tuple_new(list_of(rl.v)->items.size());
    if (!t)
        return oom(), Value();
    for (usize i = 0; i < t->len; i++)
        t->items()[i] = list_of(rl.v)->items[i];
    return obj_value(t);
}

Value pair(Value a, Value b)
{
    Root ra{ a }, rb{ b };
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom(), Value();
    t->items()[0] = ra.v.is_nil() ? value_none() : ra.v;
    t->items()[1] = rb.v.is_nil() ? value_none() : rb.v;
    return obj_value(t);
}

// `except T` for an instance: T a class, or a tuple of them.
bool matches(Value e, Value t)
{
    Value cls = type_of_value(e);
    if (cls.is_nil())
        return err_clear(), false;
    if (is_tuple(t)) {
        for (usize i = 0; i < tuple_len(t); i++)
            if (type_issub(cls, tuple_at(t, i)))
                return true;
        return false;
    }
    return type_issub(cls, t);
}

// What PySequence_Check says yes to, among what can be walked from here.
bool sequence_like(Value v)
{
    if (is_list(v) || is_tuple(v))
        return true;
    const Type *t = type_of(v);
    return t && t->getitem && !is_dict(v) && !is_inst(v);
}

bool callable(Value v)
{
    if (is_func(v) || is_native(v) || is_method(v))
        return true;
    return is_inst(v) && !type_special(v, "__call__").is_nil();
}

bool all_exc_types(Value t)
{
    if (is_tuple(t)) {
        for (usize i = 0; i < tuple_len(t); i++)
            if (!is_exc_type(tuple_at(t, i)))
                return false;
        return true;
    }
    return is_exc_type(t);
}

// ---------------------------------------------------------------- the split

enum : u32 { BY_TYPE, BY_CALL, BY_ID, KIND_MASK = 3, WANT_REST = 4 };

enum : u32 { SP_START, SP_TESTED, SP_MATCHED, SP_RESTED };

// s[0] the matcher, s[1] the stack, s[2] the leaf under test, s[3] the match
// of the group being finished; j the kind and WANT_REST. A frame is a list
// [group, next index, matches, rests].
Value frame_new(Value group, bool rest)
{
    Root rg{ group };
    ListObj *f = list_new();
    if (!f)
        return oom(), Value();
    Root rf{ obj_value(f) };
    Root m{ obj_value(list_new()) };
    Root r{ rest ? obj_value(list_new()) : value_none() };
    if (m.v.is_nil() || r.v.is_nil())
        return oom(), Value();
    if (!list_push(list_of(rf.v), rg.v) || !list_push(list_of(rf.v), Value::of_int(0)) ||
        !list_push(list_of(rf.v), m.v) || !list_push(list_of(rf.v), r.v))
        return oom(), Value();
    return rf.v;
}

ListObj *top_frame(ContObj *k)
{
    ListObj *st = list_of(k->s[1]);
    return list_of(st->items[st->items.size() - 1]);
}

// Whether a leaf matches, when that needs no call.
bool leaf_matches(ContObj *k, Value e)
{
    if ((k->j & KIND_MASK) == BY_TYPE)
        return matches(e, k->s[0]);
    // By identity: only leaves count, a group never does.
    if (is_egroup(e))
        return false;
    ListObj *ids = list_of(k->s[0]);
    for (usize i = 0; i < ids->items.size(); i++)
        if (ids->items[i] == e)
            return true;
    return false;
}

// `excs` as a group derived from `orig`: None for none, else orig.derive().
R subset(ContObj *k, Value orig, Value excs, u32 next)
{
    k->i = next;
    if (!list_of(excs)->items.size())
        return R::NotImpl;
    Root ro{ orig }, rx{ excs };
    Root derive;
    StrObj *dn = str_intern("derive");
    if (!dn)
        return oom();
    Got g = py_attr(ro.v, dn, derive.v);
    if (g == Got::Error)
        return R::Err;
    if (g != Got::Ok)
        return err_set("TypeError", "derive is not a plain method");
    return cont_call(k, derive.v, rx.v);
}

// What derive answered, with the original's cause, context and notes.
bool adopt(Value made, Value orig)
{
    if (!is_egroup(made))
        return err_set("TypeError", "derive must return an instance of BaseExceptionGroup"), false;
    Root rm{ made }, ro{ orig };
    exc_of(rm.v)->context = exc_of(ro.v)->context;
    exc_of(rm.v)->cause   = exc_of(ro.v)->cause;
    StrObj *nn            = str_intern("__notes__");
    Value notes;
    if (!nn || exc_of(ro.v)->dict.is_nil() ||
        dict_get(static_cast<DictObj *>(exc_of(ro.v)->dict.obj()), obj_value(nn), notes) != R::Ok)
        return !err_pending() || (err_clear(), true);
    if (!is_list(notes) && !is_tuple(notes))
        return true;
    Root rn{ notes };
    ListObj *copy = py_list_of(rn.v);
    if (!copy)
        return false;
    Root rc{ obj_value(copy) };
    if (exc_of(rm.v)->dict.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom(), false;
        exc_of(rm.v)->dict = obj_value(d);
    }
    return dict_set(static_cast<DictObj *>(exc_of(rm.v)->dict.obj()), obj_value(nn), rc.v) == R::Ok;
}

// A frame is finished: its results go to the one below, which NotImpl says
// the walk goes on with, or they are the answer.
R frame_done(ContObj *k, Value match, Value rest)
{
    Root rm{ match }, rr{ rest };
    ListObj *st = list_of(k->s[1]);
    st->items.pop();
    if (!st->items.size()) {
        if (k->j & WANT_REST) {
            Value p = pair(rm.v, rr.v);
            return p.is_nil() ? R::Err : cont_done(k, p);
        }
        return cont_done(k, rm.v.is_nil() ? value_none() : rm.v);
    }
    ListObj *f = top_frame(k);
    if (!rm.v.is_nil() && !list_push(list_of(f->items[2]), rm.v))
        return oom();
    if (!rr.v.is_nil() && !list_push(list_of(top_frame(k)->items[3]), rr.v))
        return oom();
    k->i = SP_TESTED;
    return R::NotImpl;
}

// One leaf's verdict. A group that did not match as a whole is walked.
R place(ContObj *k, Value e, bool hit)
{
    Root re{ e };
    ListObj *f = top_frame(k);
    if (hit)
        return list_push(list_of(f->items[2]), re.v) ? R::Ok : oom();
    if (is_egroup(re.v)) {
        Value nf = frame_new(re.v, (k->j & WANT_REST) != 0);
        if (nf.is_nil())
            return R::Err;
        Root rnf{ nf };
        return list_push(list_of(k->s[1]), rnf.v) ? R::Ok : oom();
    }
    if ((k->j & WANT_REST) && !list_push(list_of(f->items[3]), re.v))
        return oom();
    return R::Ok;
}

R split_step(ContObj *k, Value in)
{
    for (;;) {
        switch (k->i) {
        case SP_START: {
            // A leaf's verdict is back from the call.
            if (!k->s[2].is_nil()) {
                Root e{ k->s[2] };
                k->s[2] = Value();
                if (place(k, e.v, py_truth(in)) != R::Ok)
                    return R::Err;
            }
            k->i = SP_TESTED;
            continue;
        }
        case SP_TESTED: {
            ListObj *f  = top_frame(k);
            Value group = f->items[0];
            Value excs  = exc_of(group)->excs;
            u32 at      = u32(f->items[1].as_int());
            if (at < tuple_len(excs)) {
                f->items[1] = Value::of_int(i32(at + 1));
                Value e     = tuple_at(excs, at);
                if ((k->j & KIND_MASK) == BY_CALL) {
                    k->s[2] = e;
                    k->i    = SP_START;
                    return cont_call(k, k->s[0], e);
                }
                if (place(k, e, leaf_matches(k, e)) != R::Ok)
                    return R::Err;
                continue;
            }
            R r = subset(k, group, f->items[2], SP_MATCHED);
            if (r != R::NotImpl)
                return r;
            in = Value();
            [[fallthrough]];
        }
        case SP_MATCHED: {
            ListObj *f = top_frame(k);
            if (!in.is_nil() && !adopt(in, f->items[0]))
                return R::Err;
            k->s[3] = in;
            if (!(k->j & WANT_REST)) {
                R r = frame_done(k, k->s[3], Value());
                if (r != R::NotImpl)
                    return r;
                in = Value();
                continue;
            }
            R r = subset(k, f->items[0], f->items[3], SP_RESTED);
            if (r != R::NotImpl)
                return r;
            in = Value();
            [[fallthrough]];
        }
        case SP_RESTED: {
            ListObj *f = top_frame(k);
            if (!in.is_nil() && !adopt(in, f->items[0]))
                return R::Err;
            Root match{ k->s[3] };
            k->s[3] = Value();
            R r     = frame_done(k, match.v, in);
            if (r != R::NotImpl)
                return r;
            in = Value();
            continue;
        }
        }
        return err_set("SystemError", "a split lost its place");
    }
}

// A split of `self`: a pair for split(), the match alone for subgroup().
R split_start(Value self, Value matcher, u32 kind, bool rest, Value &out)
{
    Root rs{ self }, rm{ matcher };
    Root stack{ obj_value(list_new()) };
    if (stack.v.is_nil())
        return oom();
    Root kv{ cont_new(split_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = rm.v;
    k->s[1]    = stack.v;
    k->j       = kind | (rest ? WANT_REST : 0);

    // The whole group first: a match is the group itself.
    bool whole = kind == BY_TYPE && matches(rs.v, rm.v);
    if (whole) {
        out = rest ? pair(rs.v, Value()) : rs.v;
        return out.is_nil() ? R::Err : R::Ok;
    }
    Value f = frame_new(rs.v, rest);
    if (f.is_nil())
        return R::Err;
    Root rf{ f };
    if (!list_push(list_of(stack.v), rf.v))
        return oom();
    k->i = SP_TESTED;
    out  = kv.v;
    return R::Ok;
}

// s[0] the self, s[1] the condition, j WANT_REST: the call on the whole
// group, and then the walk if it said no.
R whole_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[1], k->s[0]);
    if (py_truth(in)) {
        Value r = (k->j & WANT_REST) ? pair(k->s[0], Value()) : k->s[0];
        return r.is_nil() ? R::Err : cont_done(k, r);
    }
    Root rs{ k->s[0] }, rm{ k->s[1] };
    Root stack{ obj_value(list_new()) };
    if (stack.v.is_nil())
        return oom();
    Value f = frame_new(rs.v, (k->j & WANT_REST) != 0);
    if (f.is_nil())
        return R::Err;
    Root rf{ f };
    if (!list_push(list_of(stack.v), rf.v))
        return oom();
    k->step = split_step;
    k->s[0] = rm.v;
    k->s[1] = stack.v;
    k->j    = BY_CALL | (k->j & WANT_REST);
    k->i    = SP_TESTED;
    return split_step(k, Value());
}

R split_or_subgroup(const CallArgs &a, Str who, bool rest, Value &out)
{
    if (!meth_args(a, who, 1, 1))
        return R::Err;
    Value self = a.args[0];
    if (!is_egroup(self))
        return err_set2("TypeError", "descriptor requires a 'BaseExceptionGroup' object",
                        type_name(self));
    Value m = a.args[1];
    if (!is_type(m) && callable(m)) {
        Root kv{ cont_new(whole_step) };
        if (kv.v.is_nil())
            return R::Err;
        cont_of(kv.v)->s[0] = self;
        cont_of(kv.v)->s[1] = m;
        cont_of(kv.v)->j    = rest ? WANT_REST : 0;
        out                 = kv.v;
        return R::Ok;
    }
    if (!all_exc_types(m))
        return err_set("TypeError",
                       "expected an exception type, a tuple of exception types, or "
                       "a callable (other than a class)");
    return split_start(self, m, BY_TYPE, rest, out);
}

R eg_split(const CallArgs &a, Value &out)
{
    return split_or_subgroup(a, "split", true, out);
}

R eg_subgroup(const CallArgs &a, Value &out)
{
    return split_or_subgroup(a, "subgroup", false, out);
}

R eg_derive(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "derive", 1, 1))
        return R::Err;
    if (!is_egroup(a.args[0]))
        return err_set("TypeError", "derive() requires an exception group");
    Root args{ pair(exc_of(a.args[0])->msg, a.args[1]) };
    Root cls{ exc_type_value(beg()) };
    if (args.v.is_nil() || cls.v.is_nil())
        return R::Err;
    out = egroup_new(cls.v, args.v);
    return out.is_nil() ? R::Err : R::Ok;
}

// BaseExceptionGroup.__new__(cls, message, exceptions).
R eg_dunder_new(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "BaseExceptionGroup.__new__() takes no keyword arguments");
    if (!a.nargs || !is_type(a.args[0]) || !is_egroup_type(a.args[0]))
        return err_set("TypeError", "BaseExceptionGroup.__new__(X): X is not a group type");
    TupleObj *t = tuple_new(a.nargs - 1);
    if (!t)
        return oom();
    for (u32 i = 1; i < a.nargs; i++)
        t->items()[i - 1] = a.args[i];
    Root rt{ obj_value(t) };
    out = egroup_new(a.args[0], rt.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R eg_class_getitem(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "__class_getitem__", 1, 1))
        return R::Err;
    out = genalias_new(a.args[0], a.args[1]);
    return out.is_nil() ? R::Err : R::Ok;
}

// BaseException.add_note(note): kept in __notes__, a list made on the first.
R exc_add_note(const CallArgs &a, Value &out)
{
    if (!meth_args(a, "add_note", 1, 1))
        return R::Err;
    if (!is_exc(a.args[0]))
        return err_set("TypeError", "add_note() requires an exception");
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "note must be a str, not", type_name(a.args[1]));
    Root self{ a.args[0] }, note{ a.args[1] };
    if (exc_of(self.v)->dict.is_nil()) {
        DictObj *d = dict_new();
        if (!d)
            return oom();
        exc_of(self.v)->dict = obj_value(d);
    }
    StrObj *nn = str_intern("__notes__");
    if (!nn)
        return oom();
    DictObj *d = static_cast<DictObj *>(exc_of(self.v)->dict.obj());
    Value notes;
    R r = dict_get(d, obj_value(nn), notes);
    if (r == R::Err)
        return R::Err;
    if (r == R::NotImpl) {
        ListObj *l = list_new();
        if (!l)
            return oom();
        notes = obj_value(l);
        if (dict_set(static_cast<DictObj *>(exc_of(self.v)->dict.obj()), obj_value(nn), notes) !=
            R::Ok)
            return R::Err;
    } else if (!is_list(notes)) {
        return err_set("TypeError", "Cannot add note: __notes__ is not a list");
    }
    if (!list_push(list_of(notes), note.v))
        return oom();
    out = value_none();
    return R::Ok;
}

constexpr Method GROUP_METHODS[] = {
    { "derive", eg_derive },
    { "split", eg_split },
    { "subgroup", eg_subgroup },
    { "__class_getitem__", eg_class_getitem },
};

// s[0] the bound split, s[1] the type: `except*` asks the group itself.
R match_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_call(k, k->s[0], k->s[1]);
    if (!is_tuple(in) || tuple_len(in) != 2) {
        Buf<128> b;
        b.put(type_name(k->s[2])).put(".split must return a 2-tuple, got ").put(type_name(in));
        return err_set("TypeError", b.str());
    }
    return cont_done(k, in);
}

// For PrepReraiseStar: the part of `orig` whose leaves are among `keep`'s.
R project(const CallArgs &a, Value &out)
{
    Root orig{ a.args[0] }, keep{ a.args[1] };
    Root ids{ obj_value(list_new()) };
    Root work{ obj_value(list_new()) };
    if (ids.v.is_nil() || work.v.is_nil())
        return oom();
    for (usize i = 0; i < list_of(keep.v)->items.size(); i++)
        if (!list_push(list_of(work.v), list_of(keep.v)->items[i]))
            return oom();
    while (list_of(work.v)->items.size()) {
        Root e{ list_of(work.v)->items.back() };
        list_of(work.v)->items.pop();
        if (!is_egroup(e.v)) {
            if (!list_push(list_of(ids.v), e.v))
                return oom();
            continue;
        }
        Value excs = exc_of(e.v)->excs;
        for (usize i = 0; i < tuple_len(excs); i++)
            if (!list_push(list_of(work.v), tuple_at(excs, i)))
                return oom();
    }
    return split_start(orig.v, ids.v, BY_ID, false, out);
}

bool same_metadata(Value a, Value b)
{
    StrObj *nn = str_intern("__notes__");
    Value na, nb;
    bool ha = !exc_of(a)->dict.is_nil() &&
              dict_get(static_cast<DictObj *>(exc_of(a)->dict.obj()), obj_value(nn), na) == R::Ok;
    bool hb = !exc_of(b)->dict.is_nil() &&
              dict_get(static_cast<DictObj *>(exc_of(b)->dict.obj()), obj_value(nn), nb) == R::Ok;
    return exc_of(a)->cause == exc_of(b)->cause && exc_of(a)->context == exc_of(b)->context &&
           ha == hb && (!ha || na == nb);
}

// s[0] the original, s[1] what the clauses raised, s[2] the new ones.
R reraise_step(ContObj *k, Value in)
{
    if (k->i++ == 0) {
        Root raised{ obj_value(list_new()) };
        Root again{ obj_value(list_new()) };
        if (raised.v.is_nil() || again.v.is_nil())
            return oom();
        ListObj *all = list_of(k->s[1]);
        for (usize i = 0; i < all->items.size(); i++) {
            Value e = list_of(k->s[1])->items[i];
            if (is_none(e))
                continue;
            ListObj *into = list_of(same_metadata(e, k->s[0]) ? again.v : raised.v);
            if (!list_push(into, e))
                return oom();
        }
        k->s[2] = raised.v;
        Root fn{ native_new("_project", project) };
        if (fn.v.is_nil())
            return R::Err;
        return cont_call(k, fn.v, k->s[0], 2, again.v);
    }
    Root reraised{ in };
    ListObj *raised = list_of(k->s[2]);
    if (!raised->items.size())
        return cont_done(k, reraised.v);
    if (!is_none(reraised.v) && !list_push(list_of(k->s[2]), reraised.v))
        return oom();
    raised = list_of(k->s[2]);
    if (raised->items.size() == 1)
        return cont_done(k, raised->items[0]);
    Root args{ pair(value_none(), k->s[2]) };
    Root empty{ str_new("") };
    Root cls{ exc_type_value(beg()) };
    if (args.v.is_nil() || empty.v.is_nil() || cls.v.is_nil())
        return R::Err;
    static_cast<TupleObj *>(args.v.obj())->items()[0] = empty.v;
    Value g                                           = egroup_new(cls.v, args.v);
    return g.is_nil() ? R::Err : cont_done(k, g);
}

} // namespace

bool is_egroup(Value v)
{
    return is_exc(v) && exc_is(exc_type_of(v), beg());
}

bool is_egroup_type(Value cls)
{
    return is_type(cls) && type_obj(cls)->exc && exc_is(type_obj(cls)->exc, beg());
}

Value egroup_new(Value cls, Value args)
{
    Root rc{ cls }, ra{ args };
    usize n = tuple_len(ra.v);
    if (n != 2) {
        char tmp[24];
        Buf<128> b;
        b.put("BaseExceptionGroup.__new__() takes exactly 2 arguments (");
        b.put(int_text(tmp, sizeof tmp, i64(n))).put(" given)");
        return err_set("TypeError", b.str()), Value();
    }
    Value msg = tuple_at(ra.v, 0), seq = tuple_at(ra.v, 1);
    if (!is_str(msg)) {
        Buf<128> b;
        b.put("BaseExceptionGroup.__new__() argument 1 must be str, not ").put(type_name(msg));
        return err_set("TypeError", b.str()), Value();
    }
    if (!sequence_like(seq))
        return err_set("TypeError", "second argument (exceptions) must be a sequence"), Value();
    Root list{ obj_value(py_list_of(seq)) };
    if (list.v.is_nil())
        return Value();
    Root excs{ list_tuple(list.v) };
    if (excs.v.is_nil())
        return Value();
    if (!tuple_len(excs.v))
        return err_set("ValueError", "second argument (exceptions) must be a non-empty sequence"),
               Value();
    bool nested_base         = false;
    const ExcType *exception = exc_find("Exception");
    for (usize i = 0; i < tuple_len(excs.v); i++) {
        Value e = tuple_at(excs.v, i);
        if (!is_exc(e)) {
            char tmp[24];
            Buf<128> b;
            b.put("Item ").put(int_text(tmp, sizeof tmp, i64(i)));
            b.put(" of second argument (exceptions) is not an exception");
            return err_set("ValueError", b.str()), Value();
        }
        if (!exc_is(exc_type_of(e), exception))
            nested_base = true;
    }
    Value base  = exc_type_value(beg());
    Value group = exc_type_value(exc_find("ExceptionGroup"));
    if (base.is_nil() || group.is_nil())
        return Value();
    if (rc.v == group) {
        if (nested_base)
            return err_set("TypeError", "Cannot nest BaseExceptions in an ExceptionGroup"), Value();
    } else if (rc.v == base) {
        if (!nested_base)
            rc = group;
    } else if (nested_base && exc_is(type_obj(rc.v)->exc, exception)) {
        Buf<128> b;
        b.put("Cannot nest BaseExceptions in '").put(type_obj(rc.v)->slots.name).put("'");
        return err_set("TypeError", b.str()), Value();
    }
    Value made = exc_inst(rc.v, ra.v);
    if (made.is_nil())
        return Value();
    exc_of(made)->msg  = msg;
    exc_of(made)->excs = excs.v;
    return made;
}

bool egroup_install()
{
    Root base{ exc_type_value(beg()) };
    Root top{ exc_type_value(exc_find("BaseException")) };
    if (base.v.is_nil() || top.v.is_nil())
        return false;
    DictObj *d = static_cast<DictObj *>(type_obj(base.v)->dict.obj());
    for (const Method &m : GROUP_METHODS) {
        Root fn{ native_new(m.name, m.fn) };
        StrObj *name = str_intern(m.name);
        if (fn.v.is_nil() || !name || dict_set(d, obj_value(name), fn.v) != R::Ok)
            return false;
    }
    Root nw{ native_new("__new__", eg_dunder_new) };
    Root note{ native_new("add_note", exc_add_note) };
    StrObj *nn = str_intern("__new__"), *an = str_intern("add_note");
    if (nw.v.is_nil() || note.v.is_nil() || !nn || !an)
        return false;
    return dict_set(static_cast<DictObj *>(type_obj(base.v)->dict.obj()), obj_value(nn), nw.v) ==
               R::Ok &&
           dict_set(static_cast<DictObj *>(type_obj(top.v)->dict.obj()), obj_value(an), note.v) ==
               R::Ok;
}

R egroup_match(Value exc, Value type, Value &out)
{
    if (!all_exc_types(type))
        return err_set("TypeError",
                       "catching classes that do not inherit from BaseException is not allowed");
    Root rt{ type }, re{ exc };
    bool group_type = false;
    if (is_tuple(rt.v)) {
        for (usize i = 0; i < tuple_len(rt.v); i++)
            group_type = group_type || is_egroup_type(tuple_at(rt.v, i));
    } else {
        group_type = is_egroup_type(rt.v);
    }
    if (group_type)
        return err_set("TypeError",
                       "catching ExceptionGroup with except* is not allowed. Use "
                       "except instead.");
    if (is_none(re.v)) {
        out = pair(value_none(), value_none());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (matches(re.v, rt.v)) {
        if (is_egroup(re.v)) {
            out = pair(re.v, value_none());
            return out.is_nil() ? R::Err : R::Ok;
        }
        // A naked exception is caught as a group of one.
        TupleObj *one = tuple_new(1);
        if (!one)
            return oom();
        one->items()[0] = re.v;
        Root ro{ obj_value(one) };
        Root empty{ str_new("") };
        Root cls{ exc_type_value(beg()) };
        if (empty.v.is_nil() || cls.v.is_nil())
            return R::Err;
        Root args{ pair(empty.v, ro.v) };
        if (args.v.is_nil())
            return R::Err;
        Root wrapped{ egroup_new(cls.v, args.v) };
        if (wrapped.v.is_nil())
            return R::Err;
        out = pair(wrapped.v, value_none());
        return out.is_nil() ? R::Err : R::Ok;
    }
    if (!is_egroup(re.v)) {
        out = pair(value_none(), re.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    Root split;
    StrObj *sn = str_intern("split");
    if (!sn)
        return oom();
    Got g = py_attr(re.v, sn, split.v);
    if (g == Got::Error)
        return R::Err;
    if (g != Got::Ok)
        return err_set("TypeError", "split is not a plain method");
    Root kv{ cont_new(match_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = split.v;
    cont_of(kv.v)->s[1] = rt.v;
    cont_of(kv.v)->s[2] = re.v;
    out                 = kv.v;
    return R::Ok;
}

Value egroup_reraise(Value orig, Value raised)
{
    Root ro{ orig }, rr{ raised };
    ListObj *l = list_of(rr.v);
    if (!l->items.size())
        return value_none();
    // A naked exception was caught: only one clause ran.
    if (!is_egroup(ro.v))
        return l->items[0];
    Root kv{ cont_new(reraise_step) };
    if (kv.v.is_nil())
        return Value();
    cont_of(kv.v)->s[0] = ro.v;
    cont_of(kv.v)->s[1] = rr.v;
    return kv.v;
}

namespace {

void margin(String &out, u32 n)
{
    for (u32 k = 0; k < n; k++)
        out.append("  ");
}

void rule(String &out, u32 depth, bool first, i32 number)
{
    margin(out, first ? depth + 1 : depth + 2);
    out.append(first ? "+-+" : "+");
    if (number == -1) {
        out.append("------------------------------------\n");
        return;
    }
    if (number == -2) {
        out.append("---------------- ... ----------------\n");
        return;
    }
    char tmp[24];
    out.append("---------------- ");
    out.append(int_text(tmp, sizeof tmp, i64(number)));
    out.append(" ----------------\n");
}

bool one(Value e, u32 depth, String &out);

bool group_members(Value g, u32 depth, String &out)
{
    Value excs = exc_of(g)->excs;
    usize n    = tuple_len(excs);
    usize show = n > 15 ? 15 : n;
    for (usize i = 0; i < show; i++) {
        rule(out, depth, i == 0, i32(i + 1));
        if (!one(tuple_at(excs, i), depth + 1, out))
            return false;
    }
    if (n > show) {
        rule(out, depth, false, -2);
        margin(out, depth + 2);
        char tmp[24];
        out.append("| and ");
        out.append(int_text(tmp, sizeof tmp, i64(n - show)));
        out.append(n - show == 1 ? " more exception\n" : " more exceptions\n");
    }
    rule(out, depth, false, -1);
    return true;
}

// One member, at `depth` boxes in: its line, its notes, and its own members.
bool one(Value e, u32 depth, String &out)
{
    String line;
    if (!exc_line(e, line))
        return false;
    usize at = 0;
    Str text = line.str();
    for (;;) {
        usize end = at;
        while (end < text.size() && text[end] != '\n')
            end++;
        margin(out, depth + 1);
        out.append("| ");
        out.append(text.substr(at, end - at));
        out.push('\n');
        if (end >= text.size())
            break;
        at = end + 1;
    }
    if (depth > 10)
        return true;
    return !is_egroup(e) || group_members(e, depth, out);
}

} // namespace

bool egroup_report(Value e, String &out)
{
    return group_members(e, 0, out);
}
