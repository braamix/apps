// `itertools`. Every name here is a type, as CPython's are, so `type(count(1))`
// is `itertools.count` and `chain.from_iterable` is a static method rather than
// an attribute hung on a function.
//
// Two shapes. One is lazy: `count`, `cycle`, `islice`, `chain`, `product` and
// the rest walk their source with py_next, one item per call, and are the only
// way an infinite iterator can be sliced. The other is eager, and it is
// forced: `takewhile`, `accumulate`, `groupby` and `starmap` call a function
// the program wrote, a builtin cannot make that call -- ground rule 2 -- so
// they park in a continuation, run the whole source through it, and hand back
// an iterator over the list. That is the trade map() and filter() already
// make; README.md says so.
//
// A generator or a class instance as the source is drained first, by iter_park,
// for the same reason.
#include "bigint.h"
#include "call.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
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

// A builtin cannot step a generator, so one handed in is drained first.
inline bool parks(const CallArgs &a, u32 at)
{
    return a.nargs > at && iter_needs_vm(a.args[at]);
}

// One layout for every iterator here; the type's `next` slot is what differs.
struct ItObj : Obj {
    Value src;   // the source iterator, or a list already built
    Value extra; // a second source, a fill value, the pools
    Value state; // an index vector, for the combinatorics
    i64 a, b, c; // counters: count's step, islice's bounds
    u32 at;
    bool done;
};

ItObj *it_of(Value v)
{
    return static_cast<ItObj *>(v.obj());
}

void it_trace(Obj *o)
{
    ItObj *t = static_cast<ItObj *>(o);
    gc_mark(t->src);
    gc_mark(t->extra);
    gc_mark(t->state);
}

R it_repr(Value v, String &out)
{
    Buf<64> b;
    b.put("<itertools.").put(type_name(v)).put(" object>");
    return out.append(b.str()) ? R::Ok : oom();
}

Value it_self(Value v)
{
    return v;
}

Value it_new(const Type *t, Value src, Value extra = Value())
{
    Root rs{ src }, re{ extra };
    ItObj *o = static_cast<ItObj *>(obj_alloc(t, sizeof(ItObj)));
    if (!o)
        return oom(), Value();
    o->src   = rs.v;
    o->extra = re.v;
    o->state = Value();
    o->a = o->b = o->c = 0;
    o->at              = 0;
    o->done            = false;
    return obj_value(o);
}

// The source as an iterator, refusing what does not iterate.
Value iter_arg(Value v)
{
    return py_iter(v);
}

#define IT_TYPE(var, label, nextfn)                                                        \
    constexpr Type var                                                                     \
    {                                                                                      \
        .name = label, .trace = it_trace, .repr = it_repr, .iter = it_self, .next = nextfn \
    }

// ------------------------------------------------------------- the infinite

// count(start, step): the arithmetic is the number tower's, so a count past
// the value word promotes rather than wrapping.
R count_next(Value v, Value &out)
{
    ItObj *t = it_of(v);
    out      = t->src;
    Value next;
    if (py_binop(t->src, t->extra, Op::Add, next) != R::Ok)
        return R::Err;
    it_of(v)->src = next;
    return R::Ok;
}

R cycle_next(Value v, Value &out)
{
    ItObj *t   = it_of(v);
    ListObj *l = list_of(t->src);
    if (l->items.empty())
        return R::NotImpl;
    if (t->at >= l->items.size())
        t->at = 0;
    out = l->items[t->at++];
    return R::Ok;
}

R repeat_next(Value v, Value &out)
{
    ItObj *t = it_of(v);
    if (t->a == 0)
        return R::NotImpl;
    if (t->a > 0)
        t->a--;
    out = t->src;
    return R::Ok;
}

IT_TYPE(count_type, "count", count_next);
IT_TYPE(cycle_type, "cycle", cycle_next);
IT_TYPE(repeat_type, "repeat", repeat_next);

R b_count(const CallArgs &a, Value &out)
{
    Value start = Value::of_int(0), step = Value::of_int(1);
    if (a.nargs > 2)
        return err_set("TypeError", "count() takes at most 2 arguments");
    if (a.nargs > 0)
        start = a.args[0];
    if (a.nargs > 1)
        step = a.args[1];
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n == "start")
            start = a.kwvals[k];
        else if (n == "step")
            step = a.kwvals[k];
        else
            return err_set2("TypeError", "count() got an unexpected keyword argument", n);
    }
    out = it_new(&count_type, start, step);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_cycle(const CallArgs &a, Value &out)
{
    if (!args_only(a, "cycle", 1, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_cycle, out);
    ListObj *l = py_list_of(a.args[0]);
    if (!l)
        return R::Err;
    out = it_new(&cycle_type, obj_value(l));
    return out.is_nil() ? R::Err : R::Ok;
}

R b_repeat(const CallArgs &a, Value &out)
{
    if (a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "repeat() takes 1 or 2 arguments");
    Value times = a.nargs > 1 ? a.args[1] : Value();
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n != "times")
            return err_set2("TypeError", "repeat() got an unexpected keyword argument", n);
        times = a.kwvals[k];
    }
    i64 n = -1;
    if (!times.is_nil() && !is_none(times)) {
        if (!as_index(times, n))
            return err_set("TypeError", "repeat() times must be an integer");
        if (n < 0)
            n = 0;
    }
    Root r{ it_new(&repeat_type, a.args[0]) };
    if (r.v.is_nil())
        return R::Err;
    it_of(r.v)->a = n;
    out           = r.v;
    return R::Ok;
}

// ------------------------------------------------------------- the lazy rest

// chain(*iterables): `at` is the source being walked, `extra` its iterator.
R chain_next(Value v, Value &out)
{
    Root rv{ v };
    for (;;) {
        ItObj *t = it_of(rv.v);
        if (t->extra.is_nil()) {
            ListObj *srcs = list_of(t->src);
            if (t->at >= srcs->items.size())
                return R::NotImpl;
            Value one = srcs->items[t->at++];
            Root it{ iter_arg(one) };
            if (it.v.is_nil())
                return R::Err;
            it_of(rv.v)->extra = it.v;
        }
        R r = py_next(it_of(rv.v)->extra, out);
        if (r != R::NotImpl)
            return r;
        it_of(rv.v)->extra = Value();
    }
}

R compress_next(Value v, Value &out)
{
    Root rv{ v };
    for (;;) {
        Root item, flag;
        R r = py_next(it_of(rv.v)->src, item.v);
        if (r != R::Ok)
            return r;
        r = py_next(it_of(rv.v)->extra, flag.v);
        if (r != R::Ok)
            return r;
        if (py_truth(flag.v)) {
            out = item.v;
            return R::Ok;
        }
    }
}

// islice(it, start, stop, step): `at` counts the source, `a` the next index
// wanted, `b` one past the last (-1 for none) and `c` the step.
R islice_next(Value v, Value &out)
{
    Root rv{ v };
    for (;;) {
        ItObj *t = it_of(rv.v);
        if (t->b >= 0 && i64(t->at) >= t->b)
            return R::NotImpl;
        Root got;
        R r = py_next(t->src, got.v);
        if (r != R::Ok)
            return r;
        t     = it_of(rv.v);
        i64 i = i64(t->at);
        t->at++;
        if (i == t->a) {
            t->a += t->c;
            out = got.v;
            return R::Ok;
        }
    }
}

// pairwise(it): the last item is kept in `extra`.
R pairwise_next(Value v, Value &out)
{
    Root rv{ v };
    if (it_of(rv.v)->done)
        return R::NotImpl;
    if (it_of(rv.v)->extra.is_nil()) {
        Root first;
        R r = py_next(it_of(rv.v)->src, first.v);
        if (r != R::Ok)
            return it_of(rv.v)->done = true, r;
        it_of(rv.v)->extra = first.v;
    }
    Root next;
    R r = py_next(it_of(rv.v)->src, next.v);
    if (r != R::Ok)
        return it_of(rv.v)->done = true, r;
    TupleObj *pair = tuple_new(2);
    if (!pair)
        return oom();
    pair->items()[0]   = it_of(rv.v)->extra;
    pair->items()[1]   = next.v;
    it_of(rv.v)->extra = next.v;
    out                = obj_value(pair);
    return R::Ok;
}

// zip_longest(*iters, fillvalue=): `state` marks which have run out.
R longest_next(Value v, Value &out)
{
    Root rv{ v };
    ListObj *its = list_of(it_of(rv.v)->src);
    usize n      = its->items.size();
    if (!n)
        return R::NotImpl;
    Root made{ obj_value(tuple_new(n)) };
    if (made.v.is_nil())
        return oom();
    usize alive = 0;
    for (usize i = 0; i < n; i++) {
        ListObj *flags = list_of(it_of(rv.v)->state);
        if (is_true(flags->items[i])) {
            static_cast<TupleObj *>(made.v.obj())->items()[i] = it_of(rv.v)->extra;
            continue;
        }
        Root got;
        R r = py_next(list_of(it_of(rv.v)->src)->items[i], got.v);
        if (r == R::Err)
            return R::Err;
        if (r == R::NotImpl) {
            list_of(it_of(rv.v)->state)->items[i]             = value_bool(true);
            static_cast<TupleObj *>(made.v.obj())->items()[i] = it_of(rv.v)->extra;
            continue;
        }
        alive++;
        static_cast<TupleObj *>(made.v.obj())->items()[i] = got.v;
    }
    if (!alive)
        return R::NotImpl;
    out = made.v;
    return R::Ok;
}

IT_TYPE(chain_type, "chain", chain_next);
IT_TYPE(compress_type, "compress", compress_next);
IT_TYPE(islice_type, "islice", islice_next);
IT_TYPE(pairwise_type, "pairwise", pairwise_next);
IT_TYPE(longest_type, "zip_longest", longest_next);

R b_chain(const CallArgs &a, Value &out)
{
    if (a.nkw)
        return err_set("TypeError", "chain() takes no keyword arguments");
    ListObj *l = list_new();
    if (!l)
        return oom();
    Root rl{ obj_value(l) };
    for (u32 i = 0; i < a.nargs; i++) {
        Value one = a.args[i];
        if (iter_needs_vm(one))
            return iter_park(a, i, b_chain, out);
        if (!list_push(list_of(rl.v), one))
            return oom();
    }
    out = it_new(&chain_type, rl.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_from_iterable(const CallArgs &a, Value &out)
{
    if (!args_only(a, "from_iterable", 1, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_from_iterable, out);
    ListObj *l = py_list_of(a.args[0]);
    if (!l)
        return R::Err;
    out = it_new(&chain_type, obj_value(l));
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method CHAIN_METHODS[] = { { "from_iterable", b_from_iterable, true } };

R b_compress(const CallArgs &a, Value &out)
{
    if (!args_only(a, "compress", 2, 2))
        return R::Err;
    for (u32 i = 0; i < 2; i++)
        if (parks(a, i))
            return iter_park(a, i, b_compress, out);
    Root data{ iter_arg(a.args[0]) };
    if (data.v.is_nil())
        return R::Err;
    Root sel{ iter_arg(a.args[1]) };
    if (sel.v.is_nil())
        return R::Err;
    out = it_new(&compress_type, data.v, sel.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_islice(const CallArgs &a, Value &out)
{
    if (!args_only(a, "islice", 2, 4))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_islice, out);
    i64 bounds[3] = { 0, -1, 1 };
    if (a.nargs == 2) {
        if (!is_none(a.args[1]) && (!as_index(a.args[1], bounds[1]) || bounds[1] < 0))
            return err_set("ValueError",
                           "Stop argument for islice() must be None or an integer: "
                           "0 <= x <= sys.maxsize.");
    } else {
        for (u32 i = 1; i < a.nargs; i++) {
            if (is_none(a.args[i]))
                continue;
            i64 n = 0;
            if (!as_index(a.args[i], n) || n < 0)
                return err_set("ValueError", "islice() arguments must be non-negative integers");
            bounds[i - 1] = n;
        }
        if (a.nargs < 4 || is_none(a.args[3]))
            bounds[2] = 1;
    }
    if (bounds[2] < 1)
        return err_set("ValueError", "islice() step must be at least one");
    Root it{ iter_arg(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    Root r{ it_new(&islice_type, it.v) };
    if (r.v.is_nil())
        return R::Err;
    it_of(r.v)->a = bounds[0];
    it_of(r.v)->b = bounds[1];
    it_of(r.v)->c = bounds[2];
    out           = r.v;
    return R::Ok;
}

R b_pairwise(const CallArgs &a, Value &out)
{
    if (!args_only(a, "pairwise", 1, 1))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_pairwise, out);
    Root it{ iter_arg(a.args[0]) };
    if (it.v.is_nil())
        return R::Err;
    out = it_new(&pairwise_type, it.v);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_zip_longest(const CallArgs &a, Value &out)
{
    Value fill = value_none();
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n != "fillvalue")
            return err_set2("TypeError", "zip_longest() got an unexpected keyword argument", n);
        fill = a.kwvals[k];
    }
    ListObj *its = list_new();
    if (!its)
        return oom();
    Root rl{ obj_value(its) };
    ListObj *flags = list_new();
    if (!flags)
        return oom();
    Root rf{ obj_value(flags) };
    for (u32 i = 0; i < a.nargs; i++) {
        if (parks(a, i))
            return iter_park(a, i, b_zip_longest, out);
        Root one{ iter_arg(a.args[i]) };
        if (one.v.is_nil())
            return R::Err;
        if (!list_push(list_of(rl.v), one.v) || !list_push(list_of(rf.v), value_bool(false)))
            return oom();
    }
    Root r{ it_new(&longest_type, rl.v, fill) };
    if (r.v.is_nil())
        return R::Err;
    it_of(r.v)->state = rf.v;
    out               = r.v;
    return R::Ok;
}

// ------------------------------------------------------------ combinatorics

// product, permutations, combinations and combinations_with_replacement all
// walk an odometer over one or more pools. `src` is a list of pools, `state`
// the current index vector; `a` says which rule advances it.
enum : i64 { ODO_PRODUCT, ODO_PERM, ODO_COMB, ODO_COMB_REP };

bool index_used(ListObj *state, usize upto, i32 want)
{
    for (usize i = 0; i < upto; i++)
        if (state->items[i].as_int() == want)
            return true;
    return false;
}

// The tuple the current index vector names.
Value odo_take(Value v)
{
    ItObj *t       = it_of(v);
    ListObj *state = list_of(t->state);
    ListObj *pools = list_of(t->src);
    usize n        = state->items.size();
    TupleObj *made = tuple_new(n);
    if (!made)
        return oom(), Value();
    for (usize i = 0; i < n; i++) {
        ListObj *pool    = list_of(pools->items[t->a == ODO_PRODUCT ? i : 0]);
        made->items()[i] = pool->items[usize(state->items[i].as_int())];
    }
    return obj_value(made);
}

// The next index vector, or false when the odometer has wrapped for good.
bool odo_advance(Value v)
{
    ItObj *t       = it_of(v);
    ListObj *state = list_of(t->state);
    ListObj *pools = list_of(t->src);
    usize n        = state->items.size();
    if (!n)
        return false;

    if (t->a == ODO_PRODUCT) {
        for (usize k = n; k-- > 0;) {
            i32 next = state->items[k].as_int() + 1;
            if (usize(next) < list_of(pools->items[k])->items.size()) {
                state->items[k] = Value::of_int(next);
                return true;
            }
            state->items[k] = Value::of_int(0);
        }
        return false;
    }

    usize m = list_of(pools->items[0])->items.size();
    if (t->a == ODO_COMB_REP) {
        for (usize k = n; k-- > 0;) {
            i32 next = state->items[k].as_int() + 1;
            if (usize(next) < m) {
                for (usize j = k; j < n; j++)
                    state->items[j] = Value::of_int(next);
                return true;
            }
        }
        return false;
    }
    if (t->a == ODO_COMB) {
        for (usize k = n; k-- > 0;) {
            if (usize(state->items[k].as_int()) < m - (n - k)) {
                i32 next = state->items[k].as_int() + 1;
                for (usize j = k; j < n; j++)
                    state->items[j] = Value::of_int(next + i32(j - k));
                return true;
            }
        }
        return false;
    }
    // permutations: the plain lexicographic next, skipping repeated indices.
    for (usize k = n; k-- > 0;) {
        for (i32 next = state->items[k].as_int() + 1; usize(next) < m; next++) {
            if (index_used(state, k, next))
                continue;
            state->items[k] = Value::of_int(next);
            // Fill the tail with the smallest indices still free.
            for (usize j = k + 1; j < n; j++) {
                i32 pick = 0;
                while (index_used(state, j, pick))
                    pick++;
                state->items[j] = Value::of_int(pick);
            }
            return true;
        }
    }
    return false;
}

R odo_next(Value v, Value &out)
{
    Root rv{ v };
    if (it_of(rv.v)->done)
        return R::NotImpl;
    out = odo_take(rv.v);
    if (out.is_nil())
        return R::Err;
    if (!odo_advance(rv.v))
        it_of(rv.v)->done = true;
    return R::Ok;
}

IT_TYPE(product_type, "product", odo_next);
IT_TYPE(perm_type, "permutations", odo_next);
IT_TYPE(comb_type, "combinations", odo_next);
IT_TYPE(comb_rep_type, "combinations_with_replacement", odo_next);

// `pools` is already a list of lists; `width` how many indices the vector has.
Value odo_new(const Type *t, Value pools, usize width, i64 rule, bool empty)
{
    Root rp{ pools };
    ListObj *state = list_new();
    if (!state)
        return oom(), Value();
    Root rs{ obj_value(state) };
    for (usize i = 0; i < width; i++) {
        i32 start = rule == ODO_COMB ? i32(i) : rule == ODO_PERM ? i32(i) : 0;
        if (!list_push(list_of(rs.v), Value::of_int(start)))
            return oom(), Value();
    }
    Root r{ it_new(t, rp.v) };
    if (r.v.is_nil())
        return Value();
    it_of(r.v)->state = rs.v;
    it_of(r.v)->a     = rule;
    it_of(r.v)->done  = empty;
    return r.v;
}

R b_product(const CallArgs &a, Value &out)
{
    i64 repeat = 1;
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n != "repeat")
            return err_set2("TypeError", "product() got an unexpected keyword argument", n);
        if (!as_index(a.kwvals[k], repeat) || repeat < 0)
            return err_set("ValueError", "product() repeat must be a non-negative integer");
    }
    ListObj *pools = list_new();
    if (!pools)
        return oom();
    Root rp{ obj_value(pools) };
    for (i64 r = 0; r < repeat; r++)
        for (u32 i = 0; i < a.nargs; i++) {
            if (parks(a, i))
                return iter_park(a, i, b_product, out);
            ListObj *one = py_list_of(a.args[i]);
            if (!one || !list_push(list_of(rp.v), obj_value(one)))
                return R::Err;
        }
    bool empty = false;
    for (usize i = 0; i < list_of(rp.v)->items.size(); i++)
        empty = empty || list_of(list_of(rp.v)->items[i])->items.empty();
    out = odo_new(&product_type, rp.v, list_of(rp.v)->items.size(), ODO_PRODUCT, empty);
    return out.is_nil() ? R::Err : R::Ok;
}

// The three that draw from one pool. `least` is how many indices are needed
// for the odometer to have started at all.
R one_pool(const CallArgs &a, Value &out, Str who, const Type *t, i64 rule,
           R (*again)(const CallArgs &, Value &out))
{
    if (!args_only(a, who, rule == ODO_PERM ? 1 : 2, 2))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, again, out);
    ListObj *pool = py_list_of(a.args[0]);
    if (!pool)
        return R::Err;
    Root rl{ obj_value(pool) };
    usize m = list_of(rl.v)->items.size();
    i64 r   = i64(m);
    if (a.nargs > 1 && !is_none(a.args[1])) {
        if (!as_index(a.args[1], r) || r < 0)
            return err_set("ValueError", "r must be a non-negative integer");
    }
    ListObj *pools = list_new();
    if (!pools || !list_push(pools, rl.v))
        return oom();
    Root rp{ obj_value(pools) };
    // Too few to choose from: the iterator is empty rather than an error.
    bool empty = (rule == ODO_COMB || rule == ODO_PERM) ? usize(r) > m : (r > 0 && m == 0);
    out        = odo_new(t, rp.v, usize(r), rule, empty);
    return out.is_nil() ? R::Err : R::Ok;
}

R b_permutations(const CallArgs &a, Value &out)
{
    return one_pool(a, out, "permutations", &perm_type, ODO_PERM, b_permutations);
}

R b_combinations(const CallArgs &a, Value &out)
{
    return one_pool(a, out, "combinations", &comb_type, ODO_COMB, b_combinations);
}

R b_combinations_rep(const CallArgs &a, Value &out)
{
    return one_pool(a, out, "combinations_with_replacement", &comb_rep_type, ODO_COMB_REP,
                    b_combinations_rep);
}

// --------------------------------------------------- the ones that call back

// What an eager pass is doing with each item. The continuation below runs the
// whole source through one function call per item and keeps what the rule
// says; the answer is an iterator over the list it built.
enum : u32 { EA_TAKEWHILE, EA_DROPWHILE, EA_FILTERFALSE, EA_STARMAP, EA_ACCUMULATE, EA_GROUPBY };

// s[0] the source list, s[1] the function, s[2] the output list, s[3] the
// accumulator or the current group's key; `j` is the rule and `i` the index.
R eager_step(ContObj *k, Value in);

// The list the eager pass built, walked as an ordinary sequence iterator.
Value made_over(Value list);

R start_eager(Value items, Value fn, u32 rule, Value initial, Value &out)
{
    Root ri{ items }, rf{ fn }, rv{ initial };
    ListObj *kept = list_new();
    if (!kept)
        return oom();
    Root rk{ obj_value(kept) };
    Root kv{ cont_new(eager_step) };
    if (kv.v.is_nil())
        return R::Err;
    ContObj *k = cont_of(kv.v);
    k->s[0]    = ri.v;
    k->s[1]    = rf.v;
    k->s[2]    = rk.v;
    k->s[3]    = rv.v;
    k->j       = rule;
    out        = kv.v;
    return R::Ok;
}

} // namespace

// The eager pass hands back a list to walk, under a name of its own. iter.h's
// seq_iter is exactly that, so this is map()'s trick once more.
namespace {

Value made_over(Value list)
{
    return seq_iter(list);
}

// One turn per item, driven by the VM. The loop is a loop and not a tail call
// into itself: accumulate's fast paths and groupby's "the key is the item"
// take no call at all, and a recursive step would grow the native stack by the
// length of the source.
R eager_step(ContObj *k, Value in)
{
    for (;;) {
        ListObj *xs   = list_of(k->s[0]);
        ListObj *kept = list_of(k->s[2]);
        // The answer to the call made for item i - 1.
        if (k->i > 0 && !in.is_nil()) {
            Value item = xs->items[k->i - 1];
            switch (k->j) {
            case EA_TAKEWHILE:
                if (!py_truth(in))
                    return cont_done(k, made_over(k->s[2]));
                if (!list_push(kept, item))
                    return oom();
                break;
            case EA_DROPWHILE:
                // s[3] says the dropping is over; until then nothing is kept.
                if (k->s[3].is_nil() && py_truth(in))
                    break;
                k->s[3] = value_bool(true);
                if (!list_push(kept, item))
                    return oom();
                break;
            case EA_FILTERFALSE:
                if (!py_truth(in) && !list_push(kept, item))
                    return oom();
                break;
            case EA_STARMAP:
                if (!list_push(kept, in))
                    return oom();
                break;
            case EA_ACCUMULATE:
                if (!list_push(kept, in))
                    return oom();
                k->s[3] = in;
                break;
            case EA_GROUPBY: {
                // The key of item i - 1: a new group starts where it differs.
                Root key{ in };
                bool fresh = kept->items.empty();
                if (!fresh) {
                    Value last = kept->items[kept->items.size() - 1];
                    Value prev = static_cast<TupleObj *>(last.obj())->items()[0];
                    bool same  = false;
                    if (py_eq(prev, key.v, same) != R::Ok)
                        return R::Err;
                    fresh = !same;
                }
                if (fresh) {
                    ListObj *group = list_new();
                    if (!group)
                        return oom();
                    Root rg{ obj_value(group) };
                    TupleObj *pair = tuple_new(2);
                    if (!pair)
                        return oom();
                    pair->items()[0] = key.v;
                    pair->items()[1] = rg.v;
                    if (!list_push(list_of(k->s[2]), obj_value(pair)))
                        return oom();
                }
                ListObj *out = list_of(k->s[2]);
                Value last   = out->items[out->items.size() - 1];
                if (!list_push(list_of(static_cast<TupleObj *>(last.obj())->items()[1]), item))
                    return oom();
                break;
            }
            default:
                break;
            }
        }

        if (k->i >= xs->items.size())
            return cont_done(k, made_over(k->s[2]));
        Value item = xs->items[k->i++];
        in         = Value();

        if (k->j == EA_STARMAP) {
            // The item is the argument list, so the call is f(*item).
            ListObj *args = py_list_of(item);
            if (!args)
                return R::Err;
            Root ra{ obj_value(args) };
            TupleObj *t = tuple_new(list_of(ra.v)->items.size());
            if (!t)
                return oom();
            for (usize i = 0; i < list_of(ra.v)->items.size(); i++)
                t->items()[i] = list_of(ra.v)->items[i];
            return cont_call_v(k, k->s[1], obj_value(t));
        }
        if (k->j == EA_ACCUMULATE) {
            if (k->s[3].is_nil()) {
                // The first item is the seed, with no call made for it.
                k->s[3] = item;
                if (!list_push(list_of(k->s[2]), item))
                    return oom();
                continue;
            }
            if (k->s[1].is_nil() || is_none(k->s[1])) {
                Value sum;
                if (py_binop(k->s[3], item, Op::Add, sum) != R::Ok)
                    return R::Err;
                k->s[3] = sum;
                if (!list_push(list_of(k->s[2]), sum))
                    return oom();
                continue;
            }
            return cont_call(k, k->s[1], k->s[3], 2, item);
        }
        if (k->j == EA_GROUPBY && (k->s[1].is_nil() || is_none(k->s[1]))) {
            in = item; // the key is the item itself, and there is no call
            continue;
        }
        return cont_call(k, k->s[1], item);
    }
}

R two_arg(const CallArgs &a, Value &out, Str who, u32 rule,
          R (*again)(const CallArgs &, Value &out))
{
    if (!args_only(a, who, 2, 2))
        return R::Err;
    if (parks(a, 1))
        return iter_park(a, 1, again, out);
    ListObj *xs = py_list_of(a.args[1]);
    if (!xs)
        return R::Err;
    return start_eager(obj_value(xs), a.args[0], rule, Value(), out);
}

R b_takewhile(const CallArgs &a, Value &out)
{
    return two_arg(a, out, "takewhile", EA_TAKEWHILE, b_takewhile);
}

R b_dropwhile(const CallArgs &a, Value &out)
{
    return two_arg(a, out, "dropwhile", EA_DROPWHILE, b_dropwhile);
}

R b_starmap(const CallArgs &a, Value &out)
{
    return two_arg(a, out, "starmap", EA_STARMAP, b_starmap);
}

R b_filterfalse(const CallArgs &a, Value &out)
{
    if (!args_only(a, "filterfalse", 2, 2))
        return R::Err;
    if (parks(a, 1))
        return iter_park(a, 1, b_filterfalse, out);
    ListObj *xs = py_list_of(a.args[1]);
    if (!xs)
        return R::Err;
    Root rx{ obj_value(xs) };
    // filterfalse(None, xs) keeps the items that are false.
    if (is_none(a.args[0])) {
        ListObj *kept = list_new();
        if (!kept)
            return oom();
        Root rk{ obj_value(kept) };
        for (usize i = 0; i < list_of(rx.v)->items.size(); i++)
            if (!py_truth(list_of(rx.v)->items[i]) &&
                !list_push(list_of(rk.v), list_of(rx.v)->items[i]))
                return oom();
        out = made_over(rk.v);
        return out.is_nil() ? R::Err : R::Ok;
    }
    return start_eager(rx.v, a.args[0], EA_FILTERFALSE, Value(), out);
}

R b_accumulate(const CallArgs &a, Value &out)
{
    Value fn      = Value();
    Value initial = Value();
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n == "func")
            fn = a.kwvals[k];
        else if (n == "initial")
            initial = a.kwvals[k];
        else
            return err_set2("TypeError", "accumulate() got an unexpected keyword argument", n);
    }
    if (a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "accumulate() takes 1 or 2 arguments");
    if (parks(a, 0))
        return iter_park(a, 0, b_accumulate, out);
    if (a.nargs > 1)
        fn = a.args[1];
    ListObj *xs = py_list_of(a.args[0]);
    if (!xs)
        return R::Err;
    Root rx{ obj_value(xs) };
    // An initial value is one more item in front, and it is its own first
    // answer; that is exactly what the seed rule already does.
    if (!initial.is_nil() && !is_none(initial)) {
        ListObj *with = list_new();
        if (!with || !list_push(with, initial))
            return oom();
        Root rw{ obj_value(with) };
        for (usize i = 0; i < list_of(rx.v)->items.size(); i++)
            if (!list_push(list_of(rw.v), list_of(rx.v)->items[i]))
                return oom();
        rx = rw.v;
    }
    return start_eager(rx.v, fn, EA_ACCUMULATE, Value(), out);
}

// groupby yields (key, group); the group is a list already built, which is
// what makes it safe to keep after the next key has been reached -- CPython's
// shared iterator is not.
R b_groupby(const CallArgs &a, Value &out)
{
    Value fn = Value();
    for (u32 k = 0; k < a.nkw; k++) {
        Str n = is_str(a.kwnames[k]) ? str_of(a.kwnames[k])->str() : Str();
        if (n != "key")
            return err_set2("TypeError", "groupby() got an unexpected keyword argument", n);
        fn = a.kwvals[k];
    }
    if (a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "groupby() takes 1 or 2 arguments");
    if (parks(a, 0))
        return iter_park(a, 0, b_groupby, out);
    if (a.nargs > 1)
        fn = a.args[1];
    ListObj *xs = py_list_of(a.args[0]);
    if (!xs)
        return R::Err;
    return start_eager(obj_value(xs), fn, EA_GROUPBY, Value(), out);
}

R b_tee(const CallArgs &a, Value &out)
{
    if (!args_only(a, "tee", 1, 2))
        return R::Err;
    if (parks(a, 0))
        return iter_park(a, 0, b_tee, out);
    i64 n = 2;
    if (a.nargs > 1 && (!as_index(a.args[1], n) || n < 0))
        return err_set("ValueError", "tee() n must be a non-negative integer");
    ListObj *xs = py_list_of(a.args[0]);
    if (!xs)
        return R::Err;
    Root rx{ obj_value(xs) };
    TupleObj *made = tuple_new(usize(n));
    if (!made)
        return oom();
    Root rm{ obj_value(made) };
    for (i64 i = 0; i < n; i++) {
        Value one = made_over(rx.v);
        if (one.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rm.v.obj())->items()[usize(i)] = one;
    }
    out = rm.v;
    return R::Ok;
}

struct Ctor {
    const Type *t;
    R (*fn)(const CallArgs &, Value &out);
};

constexpr Ctor CTORS[] = {
    { &count_type, b_count },       { &cycle_type, b_cycle },
    { &repeat_type, b_repeat },     { &chain_type, b_chain },
    { &compress_type, b_compress }, { &islice_type, b_islice },
    { &pairwise_type, b_pairwise }, { &longest_type, b_zip_longest },
    { &product_type, b_product },   { &perm_type, b_permutations },
    { &comb_type, b_combinations }, { &comb_rep_type, b_combinations_rep },
};

constexpr ModDef PLAIN[] = {
    { "takewhile", b_takewhile },
    { "dropwhile", b_dropwhile },
    { "starmap", b_starmap },
    { "filterfalse", b_filterfalse },
    { "accumulate", b_accumulate },
    { "groupby", b_groupby },
    { "tee", b_tee },
};

} // namespace

bool itertools_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&chain_type, CHAIN_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    for (const Ctor &c : CTORS)
        if (!mod_type(d, c.t, c.fn))
            return false;
    return mod_defs(d, PLAIN);
}
