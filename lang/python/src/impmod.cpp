// `_imp`: what importlib._bootstrap asks of the interpreter.
//
// There is nothing frozen and nothing dynamic here: the built-in modules are
// the natives, and everything else is a source file. No .pyc is written or
// read, so source_hash is only there for importlib.util.
#include "bigint.h"
#include "builtin.h"
#include "call.h"
#include "exc.h"
#include "gc.h"
#include "import.h"
#include "kernel/fmt.h"
#include "module.h"
#include "ops.h"
#include "posix.h"

namespace {

// Python 3.14's magic number, with "\r\n" after it.
constexpr i64 MAGIC_TOKEN = 168627755;

u32 lock_depth;

Str name_arg(const CallArgs &a, Str who)
{
    if (a.nargs != 1) {
        char t[16];
        Buf<96> b;
        b.put(who).put("() takes exactly one argument (").put(int_text(t, sizeof t, a.nargs));
        b.put(" given)");
        err_set("TypeError", b.str());
        return Str();
    }
    if (!is_str(a.args[0])) {
        Buf<96> b;
        b.put(who).put("() argument must be str, not ").put(type_name(a.args[0]));
        err_set("TypeError", b.str());
        return Str();
    }
    return str_of(a.args[0])->str();
}

R m_lock_held(const CallArgs &, Value &out)
{
    out = value_bool(lock_depth > 0);
    return R::Ok;
}

R m_acquire_lock(const CallArgs &, Value &out)
{
    lock_depth++;
    out = value_none();
    return R::Ok;
}

R m_release_lock(const CallArgs &, Value &out)
{
    if (!lock_depth)
        return err_set("RuntimeError", "not holding the import lock");
    lock_depth--;
    out = value_none();
    return R::Ok;
}

R m_is_builtin(const CallArgs &a, Value &out)
{
    Str name = name_arg(a, "is_builtin");
    if (err_pending())
        return R::Err;
    // sys and builtins cannot be made again.
    i32 n = name == "sys" || name == "builtins" ? -1 : native_module_known(name) ? 1 : 0;
    out   = Value::of_int(n);
    return R::Ok;
}

R m_is_frozen(const CallArgs &a, Value &out)
{
    name_arg(a, "is_frozen");
    if (err_pending())
        return R::Err;
    out = value_bool(false);
    return R::Ok;
}

R no_frozen(Value name)
{
    String r;
    if (py_repr(name, r) != R::Ok)
        return R::Err;
    Buf<160> b;
    b.put("No such frozen object named ").put(r.str());
    Root msg{ str_new(b.str()) };
    if (msg.v.is_nil())
        return R::Err;
    return exc_raise_import("ImportError", msg.v, name, Value());
}

R m_find_frozen(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "", "withdata" };
    Value v[2];
    if (!fn_take(a, "find_frozen", NAMES, 1, v))
        return R::Err;
    if (!is_str(v[0]))
        return err_set2("TypeError", "find_frozen() argument 1 must be str", type_name(v[0]));
    out = value_none();
    return R::Ok;
}

R m_get_frozen_object(const CallArgs &a, Value &out)
{
    (void)out;
    if (a.nargs < 1)
        return err_set("TypeError", "get_frozen_object expected at least 1 argument, got 0");
    return no_frozen(a.args[0]);
}

R m_init_frozen(const CallArgs &a, Value &out)
{
    (void)out;
    if (a.nargs != 1)
        return err_set("TypeError", "init_frozen() takes exactly one argument");
    return no_frozen(a.args[0]);
}

R m_is_frozen_package(const CallArgs &a, Value &out)
{
    (void)out;
    if (a.nargs != 1)
        return err_set("TypeError", "is_frozen_package() takes exactly one argument");
    return no_frozen(a.args[0]);
}

R m_frozen_module_names(const CallArgs &, Value &out)
{
    TupleObj *t = tuple_new(0);
    if (!t)
        return R::Err;
    out = obj_value(t);
    return R::Ok;
}

R m_none(const CallArgs &, Value &out)
{
    out = value_none();
    return R::Ok;
}

R m_zero(const CallArgs &, Value &out)
{
    out = Value::of_int(0);
    return R::Ok;
}

R m_override_multi(const CallArgs &, Value &out)
{
    out = Value::of_int(-1);
    return R::Ok;
}

R m_extension_suffixes(const CallArgs &, Value &out)
{
    ListObj *l = list_new();
    if (!l)
        return R::Err;
    out = obj_value(l);
    return R::Ok;
}

// create_builtin(spec): the native named spec.name. s[0] the spec.
R create_step(ContObj *k, Value in)
{
    if (k->i++ == 0)
        return cont_attr(k, k->s[0], "name");
    if (!is_str(in))
        return err_set2("TypeError", "name must be string", type_name(in));
    Str name = str_of(in)->str();
    Value m  = builtin_module(name);
    if (m.is_nil()) {
        if (err_pending())
            return R::Err;
        Buf<128> b;
        b.put("no built-in module named ").put(name);
        Root msg{ str_new(b.str()) };
        if (msg.v.is_nil())
            return R::Err;
        return exc_raise_import("ImportError", msg.v, in, Value());
    }
    return cont_done(k, m);
}

R m_create_builtin(const CallArgs &a, Value &out)
{
    if (a.nargs != 1)
        return err_set("TypeError", "create_builtin() takes exactly one argument");
    Root spec{ a.args[0] };
    Root kv{ cont_new(create_step) };
    if (kv.v.is_nil())
        return R::Err;
    cont_of(kv.v)->s[0] = spec.v;
    out                 = kv.v;
    return R::Ok;
}

R m_create_dynamic(const CallArgs &a, Value &out)
{
    (void)out;
    if (a.nargs < 1)
        return err_set("TypeError", "create_dynamic expected at least 1 argument, got 0");
    return err_set("ImportError", "extension modules are not supported");
}

// SipHash-1-3, which is _Py_KeyedHash.
u64 rotl(u64 x, u32 b)
{
    return (x << b) | (x >> (64 - b));
}

void sip_round(u64 &v0, u64 &v1, u64 &v2, u64 &v3)
{
    v0 += v1;
    v2 += v3;
    v1 = rotl(v1, 13) ^ v0;
    v3 = rotl(v3, 16) ^ v2;
    v0 = rotl(v0, 32);
    v2 += v1;
    v0 += v3;
    v1 = rotl(v1, 17) ^ v2;
    v3 = rotl(v3, 21) ^ v0;
    v2 = rotl(v2, 32);
}

u64 siphash13(u64 k0, u64 k1, Str src)
{
    usize n      = src.size();
    const u8 *in = reinterpret_cast<const u8 *>(src.data());
    u64 b        = u64(n) << 56;
    u64 v0       = k0 ^ 0x736f6d6570736575ull;
    u64 v1       = k1 ^ 0x646f72616e646f6dull;
    u64 v2       = k0 ^ 0x6c7967656e657261ull;
    u64 v3       = k1 ^ 0x7465646279746573ull;
    while (n >= 8) {
        u64 mi = 0;
        for (u32 i = 0; i < 8; i++)
            mi |= u64(in[i]) << (8 * i);
        in += 8;
        n -= 8;
        v3 ^= mi;
        sip_round(v0, v1, v2, v3);
        v0 ^= mi;
    }
    u64 t = 0;
    for (usize i = 0; i < n; i++)
        t |= u64(in[i]) << (8 * i);
    b |= t;
    v3 ^= b;
    sip_round(v0, v1, v2, v3);
    v0 ^= b;
    v2 ^= 0xff;
    sip_round(v0, v1, v2, v3);
    sip_round(v0, v1, v2, v3);
    sip_round(v0, v1, v2, v3);
    return (v0 ^ v1) ^ (v2 ^ v3);
}

R m_source_hash(const CallArgs &a, Value &out)
{
    constexpr Str NAMES[] = { "key", "source" };
    Value v[2];
    if (!fn_take(a, "source_hash", NAMES, 2, v))
        return R::Err;
    if (!is_intval(v[0])) {
        i64 ignored = 0;
        if (!as_index(v[0], ignored))
            return err_not_index(v[0]);
    }
    i64 key = 0;
    if (!int_to_i64(v[0], key))
        return err_set("OverflowError", "Python int too large to convert to C long");
    Str data;
    if (!buffer_like(v[1], data)) {
        Buf<128> b;
        b.put("source_hash() argument 'source' must be bytes-like, not ").put(type_name(v[1]));
        return err_set("TypeError", b.str());
    }
    u64 h = siphash13(u64(key), 0, data);
    char raw[8];
    for (u32 i = 0; i < 8; i++)
        raw[i] = char(h >> (8 * i));
    out = bytes_new(Str(raw, 8));
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "lock_held", m_lock_held },
    { "acquire_lock", m_acquire_lock },
    { "release_lock", m_release_lock },
    { "is_builtin", m_is_builtin },
    { "is_frozen", m_is_frozen },
    { "find_frozen", m_find_frozen },
    { "get_frozen_object", m_get_frozen_object },
    { "init_frozen", m_init_frozen },
    { "is_frozen_package", m_is_frozen_package },
    { "_frozen_module_names", m_frozen_module_names },
    { "_override_frozen_modules_for_tests", m_zero },
    { "_override_multi_interp_extensions_check", m_override_multi },
    { "_fix_co_filename", m_none },
    { "_set_lazy_attributes", m_none },
    { "extension_suffixes", m_extension_suffixes },
    { "create_builtin", m_create_builtin },
    { "exec_builtin", m_zero },
    { "create_dynamic", m_create_dynamic },
    { "exec_dynamic", m_zero },
    { "source_hash", m_source_hash },
};

} // namespace

bool imp_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    return mod_defs(d, DEFS) && mod_int(d, "pyc_magic_number_token", MAGIC_TOKEN) &&
           mod_str(d, "check_hash_based_pycs", "default");
}
