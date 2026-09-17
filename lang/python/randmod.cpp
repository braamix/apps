// `_random`: the Mersenne Twister, which is the whole of what random.py
// stands on -- everything else in that module is built from random(),
// getrandbits() and seed().
//
// MT19937 as Matsumoto and Nishimura published it, with CPython's seeding:
// an integer seed is spread over the state with init_by_array, so two
// interpreters given the same seed produce the same stream.
#include "bigint.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "method.h"
#include "module.h"
#include "ops.h"
#include "proc/rt.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

constexpr usize N    = 624;
constexpr usize M    = 397;
constexpr u32 MATRIX = 0x9908b0dfu;
constexpr u32 UPPER  = 0x80000000u;
constexpr u32 LOWER  = 0x7fffffffu;

struct RandObj : Obj {
    u32 mt[N];
    usize at;
    Value gauss; // random.py keeps its spare normal deviate on the instance
    bool has_gauss;
};

RandObj *rand_of(Value v)
{
    return static_cast<RandObj *>(v.obj());
}

void rand_trace(Obj *o)
{
    gc_mark(static_cast<RandObj *>(o)->gauss);
}

R rand_repr(Value, String &out)
{
    return out.append("<_random.Random object>") ? R::Ok : oom();
}

void init_genrand(RandObj *r, u32 s)
{
    r->mt[0] = s;
    for (usize i = 1; i < N; i++)
        r->mt[i] = 1812433253u * (r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) + u32(i);
    r->at = N;
}

void init_by_array(RandObj *r, const u32 *key, usize len)
{
    init_genrand(r, 19650218u);
    usize i = 1, j = 0;
    usize k = N > len ? N : len;
    for (; k; k--) {
        r->mt[i] =
            (r->mt[i] ^ ((r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) * 1664525u)) + key[j] + u32(j);
        i++;
        j++;
        if (i >= N) {
            r->mt[0] = r->mt[N - 1];
            i        = 1;
        }
        if (j >= len)
            j = 0;
    }
    for (k = N - 1; k; k--) {
        r->mt[i] = (r->mt[i] ^ ((r->mt[i - 1] ^ (r->mt[i - 1] >> 30)) * 1566083941u)) - u32(i);
        i++;
        if (i >= N) {
            r->mt[0] = r->mt[N - 1];
            i        = 1;
        }
    }
    r->mt[0] = 0x80000000u;
    r->at    = N;
}

u32 genrand(RandObj *r)
{
    if (r->at >= N) {
        for (usize i = 0; i < N; i++) {
            u32 y    = (r->mt[i] & UPPER) | (r->mt[(i + 1) % N] & LOWER);
            r->mt[i] = r->mt[(i + M) % N] ^ (y >> 1) ^ ((y & 1) ? MATRIX : 0);
        }
        r->at = 0;
    }
    u32 y = r->mt[r->at++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y;
}

// The key an integer seed becomes: its magnitude, 32 bits at a time.
bool seed_from_int(RandObj *r, Value n)
{
    Root abs;
    if (int_absolute(n, abs.v) != R::Ok)
        return false;
    if (abs.v.is_int()) {
        u32 key[1] = { u32(abs.v.as_int()) };
        init_by_array(r, key, 1);
        return true;
    }
    BigObj *b = big_of(abs.v);
    Vec<u32> key;
    for (u32 i = 0; i < b->len; i++)
        if (!key.push(b->limbs()[i]))
            return oom() == R::Ok;
    init_by_array(r, key.data(), key.size());
    return true;
}

// A str or bytes seed: CPython turns it into an integer first, and any
// injective map will do as long as it is stable. This is FNV-1a over the
// octets, which is what the intern table already uses.
void seed_from_text(RandObj *r, Str s)
{
    Vec<u32> key;
    u32 h = 2166136261u;
    for (usize i = 0; i < s.size(); i++) {
        h ^= u8(s[i]);
        h *= 16777619u;
        if ((i % 4) == 3)
            key.push(h);
    }
    key.push(h);
    key.push(u32(s.size()));
    init_by_array(r, key.data(), key.size());
}

R seed_now(RandObj *r, Value arg)
{
    if (arg.is_nil() || is_none(arg)) {
        // No seed: the worker's own entropy, which proc_random draws from
        // crypto.getRandomValues.
        u32 key[8];
        for (u32 &w : key)
            w = proc_random();
        init_by_array(r, key, 8);
        return R::Ok;
    }
    if (is_intval(arg))
        return seed_from_int(r, arg) ? R::Ok : R::Err;
    if (is_str(arg)) {
        seed_from_text(r, str_of(arg)->str());
        return R::Ok;
    }
    Str octets;
    if (bytes_like(arg, octets)) {
        seed_from_text(r, octets);
        return R::Ok;
    }
    return err_set2("TypeError", "seed() wants None, an int, a str or bytes", type_name(arg));
}

extern const Type random_type;

RandObj *self_rand(const CallArgs &a, Str who)
{
    Value s = a.nargs ? method_self(a.args[0]) : Value();
    if (!s.is_obj() || s.obj()->type != &random_type) {
        Buf<96> b;
        b.put(who).put("() requires a Random");
        return err_set2("TypeError", b.str(), type_name(s)), nullptr;
    }
    return rand_of(s);
}

// 53 bits, which is every double in [0, 1) and no more.
R r_random(const CallArgs &a, Value &out)
{
    RandObj *r = self_rand(a, "random");
    if (!r || !meth_args(a, "random", 0, 0))
        return R::Err;
    u32 hi = genrand(r) >> 5, lo = genrand(r) >> 6;
    out = float_new((f64(hi) * 67108864.0 + f64(lo)) * (1.0 / 9007199254740992.0));
    return out.is_nil() ? R::Err : R::Ok;
}

R r_seed(const CallArgs &a, Value &out)
{
    RandObj *r = self_rand(a, "seed");
    if (!r || !meth_args(a, "seed", 0, 2))
        return R::Err;
    if (seed_now(r, a.nargs > 1 ? a.args[1] : Value()) != R::Ok)
        return R::Err;
    rand_of(method_self(a.args[0]))->has_gauss = false;
    out                                        = value_none();
    return R::Ok;
}

// k bits as an integer, the high word first and masked, which is what makes
// getrandbits(k) uniform over [0, 2**k).
R r_getrandbits(const CallArgs &a, Value &out)
{
    RandObj *r = self_rand(a, "getrandbits");
    if (!r || !meth_args(a, "getrandbits", 1, 1))
        return R::Err;
    i64 k = 0;
    if (!as_index(a.args[1], k))
        return err_set("TypeError", "getrandbits() wants an integer");
    if (k < 0)
        return err_set("ValueError", "Cannot convert negative int");
    if (k == 0) {
        out = Value::of_int(0);
        return R::Ok;
    }
    if (k > 1u << 20)
        return err_set("OverflowError", "too many bits");
    usize words = usize((k + 31) / 32);
    Vec<u32> limbs;
    if (!limbs.reserve(words))
        return oom();
    for (usize i = 0; i < words; i++)
        limbs.push(genrand(r));
    // The top word carries only what is left over.
    u32 spare = u32(k % 32);
    if (spare)
        limbs[words - 1] >>= (32 - spare);
    out = big_make(limbs.data(), words, false);
    return out.is_nil() ? R::Err : R::Ok;
}

// The state is the 624 words and the index, which is what random.py pickles.
R r_getstate(const CallArgs &a, Value &out)
{
    RandObj *r = self_rand(a, "getstate");
    if (!r || !meth_args(a, "getstate", 0, 0))
        return R::Err;
    TupleObj *t = tuple_new(N + 1);
    if (!t)
        return oom();
    Root rt{ obj_value(t) };
    r = rand_of(method_self(a.args[0]));
    for (usize i = 0; i < N; i++) {
        Value w = int_from_i64(i64(r->mt[i]));
        if (w.is_nil())
            return R::Err;
        static_cast<TupleObj *>(rt.v.obj())->items()[i] = w;
        r                                               = rand_of(method_self(a.args[0]));
    }
    static_cast<TupleObj *>(rt.v.obj())->items()[N] = Value::of_int(i32(r->at));
    out                                             = rt.v;
    return R::Ok;
}

R r_setstate(const CallArgs &a, Value &out)
{
    RandObj *r = self_rand(a, "setstate");
    if (!r || !meth_args(a, "setstate", 1, 1))
        return R::Err;
    usize n = 0;
    if (py_len(a.args[1], n) != R::Ok)
        return R::Err;
    if (n != N + 1)
        return err_set("ValueError", "state vector is the wrong size");
    for (usize i = 0; i <= N; i++) {
        Value one;
        if (py_getitem(a.args[1], Value::of_int(i32(i)), one) != R::Ok)
            return R::Err;
        i64 w = 0;
        if (!as_index(one, w))
            return err_set("TypeError", "state vector must hold integers");
        r = rand_of(method_self(a.args[0]));
        if (i < N)
            r->mt[i] = u32(w);
        else
            r->at = usize(w) > N ? N : usize(w);
    }
    out = value_none();
    return R::Ok;
}

// random.py keeps its spare normal deviate here rather than on the instance
// dict, because the C Random has no dict of its own.
R r_getattr(Value v, StrObj *name, Value &out)
{
    if (name->str() != "gauss_next")
        return R::NotImpl;
    RandObj *r = rand_of(v);
    out        = r->has_gauss ? r->gauss : value_none();
    return R::Ok;
}

R r_setattr(Value v, StrObj *name, Value val)
{
    if (name->str() != "gauss_next")
        return err_set2("AttributeError", "Random has no such attribute", name->str());
    RandObj *r   = rand_of(v);
    r->gauss     = val.is_nil() ? Value() : val;
    r->has_gauss = !val.is_nil() && !is_none(val);
    return R::Ok;
}

constexpr Method RANDOM_METHODS[] = {
    { "random", r_random },     { "seed", r_seed },         { "getrandbits", r_getrandbits },
    { "getstate", r_getstate }, { "setstate", r_setstate },
};

constexpr Type random_type{ .name    = "_random.Random",
                            .trace   = rand_trace,
                            .repr    = rand_repr,
                            .getattr = r_getattr,
                            .setattr = r_setattr };

R b_random_new(const CallArgs &a, Value &out)
{
    if (a.nkw || a.nargs > 1)
        return err_set("TypeError", "Random() takes at most one argument");
    RandObj *r = static_cast<RandObj *>(obj_alloc(&random_type, sizeof(RandObj)));
    if (!r)
        return oom();
    r->gauss     = Value();
    r->has_gauss = false;
    r->at        = N;
    Root rr{ obj_value(r) };
    if (seed_now(rand_of(rr.v), a.nargs ? a.args[0] : Value()) != R::Ok)
        return R::Err;
    out = rr.v;
    return R::Ok;
}

} // namespace

bool random_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!method_install(&random_type, RANDOM_METHODS))
        return false;
    return mod_type(static_cast<DictObj *>(rd.v.obj()), &random_type, b_random_new);
}
