// `array`: a sequence of machine values rather than of objects.
//
// The octets are a Vec<u8> and a typecode; every item that goes in or comes
// out passes through binfmt.h, which is the same codec memoryview reads a
// non-byte view with. That is the point of the module here: it is the only
// thing in this interpreter that gives a buffer a width, and so the only thing
// that makes memoryview's itemsize, format and strides mean anything.
#include "binfmt.h"
#include "call.h"
#include "compare.h"
#include "gc.h"
#include "intern.h"
#include "iter.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

struct ArrObj : Obj {
    Vec<u8> data;
    const ItemKind *k;
};

extern const Type array_type;

bool is_array(Value v)
{
    return v.is_obj() && v.obj()->type == &array_type;
}

ArrObj *arr_of(Value v)
{
    return static_cast<ArrObj *>(v.obj());
}

void arr_fini(Obj *o)
{
    static_cast<ArrObj *>(o)->data.~Vec();
}

usize arr_count(const ArrObj *a)
{
    return a->data.size() / a->k->width;
}

Str arr_str(const ArrObj *a)
{
    return Str(reinterpret_cast<const char *>(a->data.data()), a->data.size());
}

Value arr_new(const ItemKind *k)
{
    ArrObj *a = static_cast<ArrObj *>(obj_alloc(&array_type, sizeof(ArrObj)));
    if (!a)
        return oom(), Value();
    new (&a->data) Vec<u8>();
    a->k = k;
    return obj_value(a);
}

bool arr_push(ArrObj *a, Value v)
{
    usize at = a->data.size();
    for (usize i = 0; i < a->k->width; i++)
        if (!a->data.push(0))
            return oom() == R::Ok;
    return item_put(reinterpret_cast<char *>(a->data.data()), at, a->k, v) == R::Ok;
}

R arr_len(Value v, usize &out)
{
    out = arr_count(arr_of(v));
    return R::Ok;
}

bool arr_truth(Value v)
{
    return arr_count(arr_of(v)) != 0;
}

R arr_getitem(Value v, Value key, Value &out)
{
    ArrObj *a = arr_of(v);
    usize n   = arr_count(a);
    if (is_slice(key)) {
        i64 start = 0, stop = 0, step = 1;
        usize count = 0;
        if (!slice_resolve(key, n, start, stop, step, count))
            return R::Err;
        Root rv{ v };
        Root made{ arr_new(arr_of(rv.v)->k) };
        if (made.v.is_nil())
            return R::Err;
        for (usize i = 0; i < count; i++) {
            ArrObj *src = arr_of(rv.v);
            Value one =
                item_get(arr_str(src), usize(start + i64(i) * step) * src->k->width, src->k);
            if (one.is_nil())
                return R::Err;
            Root ro{ one };
            if (!arr_push(arr_of(made.v), ro.v))
                return R::Err;
        }
        out = made.v;
        return R::Ok;
    }
    usize i = 0;
    if (index_of(key, n, i) != R::Ok)
        return R::Err;
    out = item_get(arr_str(a), i * a->k->width, a->k);
    return out.is_nil() ? R::Err : R::Ok;
}

R arr_setitem(Value v, Value key, Value item)
{
    ArrObj *a = arr_of(v);
    usize i   = 0;
    if (index_of(key, arr_count(a), i) != R::Ok)
        return R::Err;
    return item_put(reinterpret_cast<char *>(a->data.data()), i * a->k->width, a->k, item);
}

R arr_delitem(Value v, Value key)
{
    ArrObj *a = arr_of(v);
    usize i   = 0;
    if (index_of(key, arr_count(a), i) != R::Ok)
        return R::Err;
    usize w = a->k->width;
    for (usize j = i * w; j + w < a->data.size(); j++)
        a->data[j] = a->data[j + w];
    for (usize j = 0; j < w; j++)
        a->data.pop();
    return R::Ok;
}

// The items, as a list: the comparisons and the searches go through it.
ListObj *arr_items(Value v)
{
    Root rv{ v };
    ListObj *l = list_new();
    if (!l)
        return oom(), nullptr;
    Root rl{ obj_value(l) };
    for (usize i = 0; i < arr_count(arr_of(rv.v)); i++) {
        ArrObj *a = arr_of(rv.v);
        Value one = item_get(arr_str(a), i * a->k->width, a->k);
        if (one.is_nil() || !list_push(list_of(rl.v), one))
            return nullptr;
    }
    return list_of(rl.v);
}

R arr_eq(Value a, Value b, bool &out)
{
    if (!is_array(b))
        return R::NotImpl;
    // Two arrays are equal when their items are, whatever their typecodes:
    // array('i', [1]) == array('d', [1.0]) is True, as CPython says.
    Root ra{ a }, rb{ b };
    ListObj *x = arr_items(ra.v);
    if (!x)
        return R::Err;
    Root rx{ obj_value(x) };
    ListObj *y = arr_items(rb.v);
    if (!y)
        return R::Err;
    Root ry{ obj_value(y) };
    return seq_eq(list_of(rx.v)->items.data(), list_of(rx.v)->items.size(),
                  list_of(ry.v)->items.data(), list_of(ry.v)->items.size(), out);
}

R arr_order(Value a, Value b, Cmp op, bool &out)
{
    if (!is_array(b))
        return R::NotImpl;
    Root ra{ a }, rb{ b };
    ListObj *x = arr_items(ra.v);
    if (!x)
        return R::Err;
    Root rx{ obj_value(x) };
    ListObj *y = arr_items(rb.v);
    if (!y)
        return R::Err;
    Root ry{ obj_value(y) };
    return seq_order(list_of(rx.v)->items.data(), list_of(rx.v)->items.size(),
                     list_of(ry.v)->items.data(), list_of(ry.v)->items.size(), op, out);
}

R arr_contains(Value v, Value item, bool &out)
{
    Root rv{ v };
    ListObj *xs = arr_items(rv.v);
    if (!xs)
        return R::Err;
    return seq_contains(xs->items.data(), xs->items.size(), item, out);
}

Value arr_iter(Value v)
{
    ListObj *l = arr_items(v);
    return l ? seq_iter(obj_value(l)) : Value();
}

R arr_repr(Value v, String &out)
{
    Root rv{ v };
    char code = arr_of(rv.v)->k->code;
    if (!out.append("array('") || !out.push(code) || !out.push('\''))
        return oom();
    usize n = arr_count(arr_of(rv.v));
    if (!n)
        return out.push(')') ? R::Ok : oom();
    // A 'u' array prints as a str, which is what tounicode answers.
    if (code == 'u') {
        String text;
        for (usize i = 0; i < n; i++) {
            ArrObj *a = arr_of(rv.v);
            Value one = item_get(arr_str(a), i * a->k->width, a->k);
            if (one.is_nil() || !text.append(str_of(one)->str()))
                return R::Err;
        }
        Root s{ str_new(text.str()) };
        if (s.v.is_nil())
            return R::Err;
        if (!out.append(", "))
            return oom();
        if (py_repr(s.v, out) != R::Ok)
            return R::Err;
        return out.push(')') ? R::Ok : oom();
    }
    if (!out.append(", ["))
        return oom();
    for (usize i = 0; i < n; i++) {
        if (i && !out.append(", "))
            return oom();
        ArrObj *a = arr_of(rv.v);
        Value one = item_get(arr_str(a), i * a->k->width, a->k);
        if (one.is_nil() || py_repr(one, out) != R::Ok)
            return R::Err;
    }
    return out.append("])") ? R::Ok : oom();
}

R arr_binop(Value a, Value b, Op op, Value &out)
{
    if (op == Op::Add) {
        if (!is_array(a) || !is_array(b))
            return R::NotImpl;
        if (arr_of(a)->k != arr_of(b)->k)
            return err_set("TypeError", "can only append arrays of the same kind");
        Root ra{ a }, rb{ b };
        Root made{ arr_new(arr_of(ra.v)->k) };
        if (made.v.is_nil())
            return R::Err;
        Value both[2] = { ra.v, rb.v };
        for (Value one : both)
            for (usize i = 0; i < arr_of(one)->data.size(); i++)
                if (!arr_of(made.v)->data.push(arr_of(one)->data[i]))
                    return oom();
        out = made.v;
        return R::Ok;
    }
    if (op != Op::Mul)
        return R::NotImpl;
    Value seq = is_array(a) ? a : b;
    Value n   = is_array(a) ? b : a;
    i64 times = 0;
    if (!as_index(n, times))
        return R::NotImpl;
    Root rs{ seq };
    Root made{ arr_new(arr_of(rs.v)->k) };
    if (made.v.is_nil())
        return R::Err;
    for (i64 k = 0; k < times; k++)
        for (usize i = 0; i < arr_of(rs.v)->data.size(); i++)
            if (!arr_of(made.v)->data.push(arr_of(rs.v)->data[i]))
                return oom();
    out = made.v;
    return R::Ok;
}

ArrObj *self_arr(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!is_array(s)) {
        Buf<96> b;
        b.put(who).put("() requires an array");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return arr_of(s);
}

R a_append(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "append");
    if (!x || !meth_args(a, "append", 1, 1))
        return R::Err;
    if (!arr_push(x, a.args[1]))
        return R::Err;
    out = value_none();
    return R::Ok;
}

R extend_from(Value self, Value src)
{
    Root rs{ self }, rv{ src };
    if (is_array(rv.v)) {
        if (arr_of(rv.v)->k != arr_of(rs.v)->k)
            return err_set("TypeError", "can only extend with an array of the same kind");
        // A copy first: extending an array with itself would walk what it is
        // growing.
        Vec<u8> copy;
        for (usize i = 0; i < arr_of(rv.v)->data.size(); i++)
            if (!copy.push(arr_of(rv.v)->data[i]))
                return oom();
        for (usize i = 0; i < copy.size(); i++)
            if (!arr_of(rs.v)->data.push(copy[i]))
                return oom();
        return R::Ok;
    }
    ListObj *items = py_list_of(rv.v);
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };
    for (usize i = 0; i < list_of(ri.v)->items.size(); i++)
        if (!arr_push(arr_of(rs.v), list_of(ri.v)->items[i]))
            return R::Err;
    return R::Ok;
}

R a_extend(const CallArgs &a, Value &out)
{
    if (!self_arr(a, "extend") || !meth_args(a, "extend", 1, 1))
        return R::Err;
    if (iter_needs_vm(a.args[1]))
        return iter_park(a, 1, a_extend, out);
    if (extend_from(method_self(a.args[0]), a.args[1]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R a_fromlist(const CallArgs &a, Value &out)
{
    if (!self_arr(a, "fromlist") || !meth_args(a, "fromlist", 1, 1))
        return R::Err;
    if (!is_list(a.args[1]))
        return err_set2("TypeError", "fromlist() wants a list", type_name(a.args[1]));
    if (extend_from(method_self(a.args[0]), a.args[1]) != R::Ok)
        return R::Err;
    out = value_none();
    return R::Ok;
}

R a_tolist(const CallArgs &a, Value &out)
{
    if (!self_arr(a, "tolist") || !meth_args(a, "tolist", 0, 0))
        return R::Err;
    ListObj *l = arr_items(method_self(a.args[0]));
    if (!l)
        return R::Err;
    out = obj_value(l);
    return R::Ok;
}

R a_tobytes(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "tobytes");
    if (!x || !meth_args(a, "tobytes", 0, 0))
        return R::Err;
    out = bytes_new(arr_str(x));
    return out.is_nil() ? R::Err : R::Ok;
}

R a_frombytes(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "frombytes");
    if (!x || !meth_args(a, "frombytes", 1, 1))
        return R::Err;
    Str octets;
    if (!bytes_like(a.args[1], octets))
        return err_set2("TypeError", "frombytes() wants a bytes-like object", type_name(a.args[1]));
    if (octets.size() % x->k->width)
        return err_set("ValueError", "bytes length is not a multiple of the item size");
    for (usize i = 0; i < octets.size(); i++)
        if (!x->data.push(u8(octets[i])))
            return oom();
    out = value_none();
    return R::Ok;
}

R a_tounicode(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "tounicode");
    if (!x || !meth_args(a, "tounicode", 0, 0))
        return R::Err;
    if (x->k->code != 'u')
        return err_set("ValueError", "tounicode() wants a 'u' array");
    Root self{ method_self(a.args[0]) };
    String text;
    for (usize i = 0; i < arr_count(arr_of(self.v)); i++) {
        ArrObj *now = arr_of(self.v);
        Value one   = item_get(arr_str(now), i * now->k->width, now->k);
        if (one.is_nil() || !text.append(str_of(one)->str()))
            return R::Err;
    }
    out = str_new(text.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R a_fromunicode(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "fromunicode");
    if (!x || !meth_args(a, "fromunicode", 1, 1))
        return R::Err;
    if (x->k->code != 'u')
        return err_set("ValueError", "fromunicode() wants a 'u' array");
    if (!is_str(a.args[1]))
        return err_set2("TypeError", "fromunicode() wants a str", type_name(a.args[1]));
    StrObj *s = str_of(a.args[1]);
    for (usize i = 0; i < s->chars; i++) {
        char32_t c = str_char_at(s, i);
        usize at   = x->data.size();
        for (usize j = 0; j < 4; j++)
            if (!x->data.push(0))
                return oom();
        for (usize j = 0; j < 4; j++)
            x->data[at + j] = u8(u32(c) >> (8 * j));
    }
    out = value_none();
    return R::Ok;
}

R a_byteswap(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "byteswap");
    if (!x || !meth_args(a, "byteswap", 0, 0))
        return R::Err;
    usize w = x->k->width;
    for (usize i = 0; i + w <= x->data.size(); i += w)
        for (usize j = 0; j < w / 2; j++) {
            u8 t                   = x->data[i + j];
            x->data[i + j]         = x->data[i + w - 1 - j];
            x->data[i + w - 1 - j] = t;
        }
    out = value_none();
    return R::Ok;
}

R a_buffer_info(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "buffer_info");
    if (!x || !meth_args(a, "buffer_info", 0, 0))
        return R::Err;
    TupleObj *t = tuple_new(2);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    x         = arr_of(method_self(a.args[0]));
    Value ptr = int_from_i64(i64(usize(x->data.data())));
    if (ptr.is_nil())
        return R::Err;
    static_cast<TupleObj *>(rt.v.obj())->items()[0] = ptr;
    static_cast<TupleObj *>(rt.v.obj())->items()[1] = Value::of_int(i32(arr_count(x)));
    out                                             = rt.v;
    return R::Ok;
}

R a_reverse(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "reverse");
    if (!x || !meth_args(a, "reverse", 0, 0))
        return R::Err;
    usize w = x->k->width;
    usize n = arr_count(x);
    for (usize i = 0; i + 1 < n - i; i++)
        for (usize j = 0; j < w; j++) {
            u8 t                         = x->data[i * w + j];
            x->data[i * w + j]           = x->data[(n - 1 - i) * w + j];
            x->data[(n - 1 - i) * w + j] = t;
        }
    out = value_none();
    return R::Ok;
}

R a_pop(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "pop");
    if (!x || !meth_args(a, "pop", 0, 1))
        return R::Err;
    usize n = arr_count(x);
    if (!n)
        return err_set("IndexError", "pop from an empty array");
    i64 at = i64(n) - 1;
    if (a.nargs > 1 && !as_index(a.args[1], at))
        return err_set("TypeError", "pop() wants an integer index");
    if (at < 0)
        at += i64(n);
    if (at < 0 || at >= i64(n))
        return err_set("IndexError", "pop index out of range");
    out = item_get(arr_str(x), usize(at) * x->k->width, x->k);
    if (out.is_nil())
        return R::Err;
    Root ro{ out };
    x       = arr_of(method_self(a.args[0]));
    usize w = x->k->width;
    for (usize j = usize(at) * w; j + w < x->data.size(); j++)
        x->data[j] = x->data[j + w];
    for (usize j = 0; j < w; j++)
        x->data.pop();
    out = ro.v;
    return R::Ok;
}

R a_insert(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "insert");
    if (!x || !meth_args(a, "insert", 2, 2))
        return R::Err;
    i64 at = 0;
    if (!as_index(a.args[1], at))
        return err_set("TypeError", "insert() wants an integer index");
    usize n = arr_count(x);
    if (at < 0)
        at += i64(n);
    if (at < 0)
        at = 0;
    if (at > i64(n))
        at = i64(n);
    if (!arr_push(x, a.args[2]))
        return R::Err;
    // The new item went on the end; roll it back to where it belongs.
    x       = arr_of(method_self(a.args[0]));
    usize w = x->k->width;
    for (usize i = arr_count(x) - 1; i > usize(at); i--)
        for (usize j = 0; j < w; j++) {
            u8 t                     = x->data[i * w + j];
            x->data[i * w + j]       = x->data[(i - 1) * w + j];
            x->data[(i - 1) * w + j] = t;
        }
    out = value_none();
    return R::Ok;
}

R search(const CallArgs &a, Value &out, u32 what, Str who)
{
    if (!self_arr(a, who))
        return R::Err;
    ListObj *items = arr_items(method_self(a.args[0]));
    if (!items)
        return R::Err;
    Root ri{ obj_value(items) };
    out = cmp_find(ri.v, a.args[1], what, 0, list_of(ri.v)->items.size());
    return out.is_nil() ? R::Err : R::Ok;
}

R a_count(const CallArgs &a, Value &out)
{
    return meth_args(a, "count", 1, 1) ? search(a, out, CMP_COUNT, "count") : R::Err;
}

R a_index(const CallArgs &a, Value &out)
{
    return meth_args(a, "index", 1, 1) ? search(a, out, CMP_INDEX, "index") : R::Err;
}

R a_remove(const CallArgs &a, Value &out)
{
    ArrObj *x = self_arr(a, "remove");
    if (!x || !meth_args(a, "remove", 1, 1))
        return R::Err;
    Root self{ method_self(a.args[0]) };
    for (usize i = 0; i < arr_count(arr_of(self.v)); i++) {
        ArrObj *now = arr_of(self.v);
        Value one   = item_get(arr_str(now), i * now->k->width, now->k);
        if (one.is_nil())
            return R::Err;
        bool same = false;
        if (py_eq(one, a.args[1], same) != R::Ok)
            return R::Err;
        if (same) {
            if (arr_delitem(self.v, Value::of_int(i32(i))) != R::Ok)
                return R::Err;
            out = value_none();
            return R::Ok;
        }
    }
    return err_set("ValueError", "array.remove(x): x not in array");
}

R a_getattr(Value v, StrObj *name, Value &out)
{
    ArrObj *x = arr_of(v);
    Str n     = name->str();
    if (n == "typecode") {
        char c = x->k->code;
        out    = str_new(Str(&c, 1));
    } else if (n == "itemsize") {
        out = Value::of_int(x->k->width);
    } else {
        return R::NotImpl;
    }
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr Method ARRAY_METHODS[] = {
    { "append", a_append },       { "extend", a_extend },
    { "fromlist", a_fromlist },   { "tolist", a_tolist },
    { "tobytes", a_tobytes },     { "frombytes", a_frombytes },
    { "tounicode", a_tounicode }, { "fromunicode", a_fromunicode },
    { "byteswap", a_byteswap },   { "buffer_info", a_buffer_info },
    { "reverse", a_reverse },     { "pop", a_pop },
    { "insert", a_insert },       { "count", a_count },
    { "index", a_index },         { "remove", a_remove },
};

constexpr Type array_type{ .name     = "array",
                           .fini     = arr_fini,
                           .truth    = arr_truth,
                           .eq       = arr_eq,
                           .order    = arr_order,
                           .repr     = arr_repr,
                           .len      = arr_len,
                           .getitem  = arr_getitem,
                           .setitem  = arr_setitem,
                           .delitem  = arr_delitem,
                           .contains = arr_contains,
                           .binop    = arr_binop,
                           .iter     = arr_iter,
                           .getattr  = a_getattr,
                           .patma    = PATMA_SEQ };

R b_array(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs < 1 || a.nargs > 2)
        return err_set("TypeError", "array() takes a typecode and an optional initialiser");
    if (!is_str(a.args[0]) || str_of(a.args[0])->chars != 1)
        return err_set("TypeError", "array() argument 1 must be a unicode character");
    const ItemKind *k = item_kind(str_of(a.args[0])->bytes()[0]);
    if (!k || k->code == '?' || k->code == 'c')
        return err_set("ValueError",
                       "bad typecode (must be b, B, u, h, H, i, I, l, L, q, Q, f or d)");
    if (a.nargs > 1 && iter_needs_vm(a.args[1]))
        return iter_park(a, 1, b_array, out);
    Root made{ arr_new(k) };
    if (made.v.is_nil())
        return R::Err;
    if (a.nargs > 1) {
        // A str fills a 'u' array; bytes fill any array from its octets.
        Str octets;
        if (is_str(a.args[1])) {
            CallArgs b;
            Value av[2] = { made.v, a.args[1] };
            b.args      = av;
            b.nargs     = 2;
            Value ignored;
            if (a_fromunicode(b, ignored) != R::Ok)
                return R::Err;
        } else if (bytes_like(a.args[1], octets) && !is_array(a.args[1])) {
            CallArgs b;
            Value av[2] = { made.v, a.args[1] };
            b.args      = av;
            b.nargs     = 2;
            Value ignored;
            if (a_frombytes(b, ignored) != R::Ok)
                return R::Err;
        } else if (is_array(a.args[1]) && arr_of(a.args[1])->k != k) {
            // An array of another typecode converts item by item here, where
            // extend() would refuse it: the constructor is where a width may
            // change.
            ListObj *items = arr_items(a.args[1]);
            if (!items)
                return R::Err;
            Root ri{ obj_value(items) };
            for (usize i = 0; i < list_of(ri.v)->items.size(); i++)
                if (!arr_push(arr_of(made.v), list_of(ri.v)->items[i]))
                    return R::Err;
        } else if (extend_from(made.v, a.args[1]) != R::Ok) {
            return R::Err;
        }
    }
    out = made.v;
    return R::Ok;
}

constexpr ModDef DEFS[] = { { "_array_reconstructor", b_array } };

} // namespace

bool array_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&array_type, ARRAY_METHODS))
        return false;
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!mod_type(d, &array_type, b_array) || !mod_defs(d, DEFS))
        return false;
    // The two names array.py's own users read: the codes and the class under
    // its other name.
    Root w{ type_wrap(&array_type) };
    return !w.v.is_nil() && mod_put(d, "ArrayType", w.v) &&
           mod_str(d, "typecodes", "bBuhHiIlLqQfd");
}

// memoryview reads an array's octets and its typecode through these.
bool array_bytes(Value v, Str &out, char &code)
{
    if (!is_array(v))
        return false;
    out  = arr_str(arr_of(v));
    code = arr_of(v)->k->code;
    return true;
}

u8 *array_data(Value v)
{
    return is_array(v) ? arr_of(v)->data.data() : nullptr;
}
