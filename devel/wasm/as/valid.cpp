#include "valid.h"

#include "kernel/hash.h"
#include "optable.h"
#include "out.h"

using namespace wat;

namespace {

// ------------------------------------------------------------ types

// A value type's kind; bottom is a type not known, on an unreachable stack.
enum : u8 { VI32, VI64, VF32, VF64, VV128, VREF, VBOT };

// A heap type: a concrete type's index below H_ABS, an abstract one from
// it, and bottom.
constexpr u32 H_ABS = 0xfffffff0;
constexpr u32 H_BOT = 0xffffffff;

u32 abs_heap(AbsHeapType a)
{
    return H_ABS + u32(a);
}

bool is_abs(u32 h, AbsHeapType a)
{
    return h == abs_heap(a);
}

struct VT {
    u8 kind   = VBOT;
    bool null = false;
    u32 heap  = H_BOT;
};

VT num(u8 k)
{
    VT t;
    t.kind = k;
    return t;
}

VT ref(bool null, u32 heap)
{
    VT t;
    t.kind = VREF;
    t.null = null;
    t.heap = heap;
    return t;
}

VT ref_of(const RefType &r)
{
    return ref(r.nullable,
               r.heap.kind == HeapType::Kind::Abstract ? abs_heap(r.heap.abs) : r.heap.index.value);
}

VT val_of(const ValType &t)
{
    return t.kind == ValType::Kind::Ref ? ref_of(t.type) : num(u8(t.kind));
}

VT unpacked(const StorageType &s)
{
    return s.kind == StorageType::Kind::Value ? val_of(s.type) : num(VI32);
}

u8 addr_kind(AddrType a)
{
    return a == AddrType::Addr64 ? VI64 : VI32;
}

constexpr Str ABS_NAME[] = { "any",  "eq",     "i31", "struct", "array",  "none",
                             "func", "nofunc", "exn", "noexn",  "extern", "noextern" };
constexpr Str NUM_NAME[] = { "i32", "i64", "f32", "f64", "v128" };

void put_heap(Out &o, u32 h)
{
    if (h == H_BOT)
        o.put("something");
    else if (h >= H_ABS)
        o.put(ABS_NAME[h - H_ABS]);
    else
        o.num(h);
}

void put_type(Out &o, VT t)
{
    if (t.kind == VBOT) {
        o.put("bot");
    } else if (t.kind != VREF) {
        o.put(NUM_NAME[t.kind]);
    } else {
        o.put("(ref ");
        if (t.null)
            o.put("null ");
        put_heap(o, t.heap);
        o.put(')');
    }
}

void put_types(Out &o, const VT *t, u32 n)
{
    o.put('[');
    for (u32 i = 0; i < n; i++) {
        if (i)
            o.put(' ');
        put_type(o, t[i]);
    }
    o.put(']');
}

// ------------------------------------------------------------ instructions

// What an instruction does, beyond the shape of its immediates.
enum Code : u8 {
    C_NUM, // pops `in`, pushes `out`
    C_UNREACHABLE,
    C_NOP,
    C_DROP,
    C_RETURN,
    C_THROW,
    C_THROW_REF,
    C_RETHROW,
    C_BR,
    C_BR_IF,
    C_BR_ON_NULL,
    C_BR_ON_NON_NULL,
    C_BR_ON_CAST,
    C_BR_ON_CAST_FAIL,
    C_CALL,
    C_RETURN_CALL,
    C_CALL_REF,
    C_RETURN_CALL_REF,
    C_CALL_INDIRECT,
    C_RETURN_CALL_INDIRECT,
    C_LOCAL_GET,
    C_LOCAL_SET,
    C_LOCAL_TEE,
    C_GLOBAL_GET,
    C_GLOBAL_SET,
    C_TABLE_GET,
    C_TABLE_SET,
    C_TABLE_SIZE,
    C_TABLE_GROW,
    C_TABLE_FILL,
    C_TABLE_COPY,
    C_TABLE_INIT,
    C_ELEM_DROP,
    C_MEMORY_SIZE,
    C_MEMORY_GROW,
    C_MEMORY_FILL,
    C_MEMORY_COPY,
    C_MEMORY_INIT,
    C_DATA_DROP,
    C_REF_IS_NULL,
    C_REF_AS_NON_NULL,
    C_REF_EQ,
    C_REF_FUNC,
    C_REF_I31,
    C_I31_GET,
    C_REF_TEST,
    C_REF_CAST,
    C_STRUCT_NEW,
    C_STRUCT_NEW_DEFAULT,
    C_STRUCT_GET,
    C_STRUCT_GET_PACKED,
    C_STRUCT_SET,
    C_ARRAY_NEW,
    C_ARRAY_NEW_DEFAULT,
    C_ARRAY_NEW_DATA,
    C_ARRAY_NEW_ELEM,
    C_ARRAY_GET,
    C_ARRAY_GET_PACKED,
    C_ARRAY_SET,
    C_ARRAY_LEN,
    C_ARRAY_FILL,
    C_ARRAY_COPY,
    C_ARRAY_INIT_DATA,
    C_ARRAY_INIT_ELEM,
    C_ANY_CONVERT,
    C_EXTERN_CONVERT,
    C_LOAD, // memory accesses: `vt` is the value's type
    C_STORE,
    C_RMW,
    C_CMPXCHG,
    C_NOTIFY,
    C_WAIT32,
    C_WAIT64,
    C_LOAD_LANE,
    C_STORE_LANE,
    C_EXTRACT, // lanes: `vt` is the lane's type
    C_REPLACE,
    C_OTHER, // its Instr kind says
};

struct Named {
    Str name;
    Code code;
};

constexpr Named NAMED[] = {
    { "unreachable", C_UNREACHABLE },
    { "nop", C_NOP },
    { "drop", C_DROP },
    { "return", C_RETURN },
    { "throw", C_THROW },
    { "throw_ref", C_THROW_REF },
    { "rethrow", C_RETHROW },
    { "br", C_BR },
    { "br_if", C_BR_IF },
    { "br_on_null", C_BR_ON_NULL },
    { "br_on_non_null", C_BR_ON_NON_NULL },
    { "br_on_cast", C_BR_ON_CAST },
    { "br_on_cast_fail", C_BR_ON_CAST_FAIL },
    { "call", C_CALL },
    { "return_call", C_RETURN_CALL },
    { "call_ref", C_CALL_REF },
    { "return_call_ref", C_RETURN_CALL_REF },
    { "call_indirect", C_CALL_INDIRECT },
    { "return_call_indirect", C_RETURN_CALL_INDIRECT },
    { "local.get", C_LOCAL_GET },
    { "local.set", C_LOCAL_SET },
    { "local.tee", C_LOCAL_TEE },
    { "global.get", C_GLOBAL_GET },
    { "global.set", C_GLOBAL_SET },
    { "table.get", C_TABLE_GET },
    { "table.set", C_TABLE_SET },
    { "table.size", C_TABLE_SIZE },
    { "table.grow", C_TABLE_GROW },
    { "table.fill", C_TABLE_FILL },
    { "table.copy", C_TABLE_COPY },
    { "table.init", C_TABLE_INIT },
    { "elem.drop", C_ELEM_DROP },
    { "memory.size", C_MEMORY_SIZE },
    { "memory.grow", C_MEMORY_GROW },
    { "memory.fill", C_MEMORY_FILL },
    { "memory.copy", C_MEMORY_COPY },
    { "memory.init", C_MEMORY_INIT },
    { "data.drop", C_DATA_DROP },
    { "ref.is_null", C_REF_IS_NULL },
    { "ref.as_non_null", C_REF_AS_NON_NULL },
    { "ref.eq", C_REF_EQ },
    { "ref.func", C_REF_FUNC },
    { "ref.i31", C_REF_I31 },
    { "i31.get_s", C_I31_GET },
    { "i31.get_u", C_I31_GET },
    { "ref.test", C_REF_TEST },
    { "ref.cast", C_REF_CAST },
    { "struct.new", C_STRUCT_NEW },
    { "struct.new_default", C_STRUCT_NEW_DEFAULT },
    { "struct.get", C_STRUCT_GET },
    { "struct.get_s", C_STRUCT_GET_PACKED },
    { "struct.get_u", C_STRUCT_GET_PACKED },
    { "struct.set", C_STRUCT_SET },
    { "array.new", C_ARRAY_NEW },
    { "array.new_default", C_ARRAY_NEW_DEFAULT },
    { "array.new_data", C_ARRAY_NEW_DATA },
    { "array.new_elem", C_ARRAY_NEW_ELEM },
    { "array.get", C_ARRAY_GET },
    { "array.get_s", C_ARRAY_GET_PACKED },
    { "array.get_u", C_ARRAY_GET_PACKED },
    { "array.set", C_ARRAY_SET },
    { "array.len", C_ARRAY_LEN },
    { "array.fill", C_ARRAY_FILL },
    { "array.copy", C_ARRAY_COPY },
    { "array.init_data", C_ARRAY_INIT_DATA },
    { "array.init_elem", C_ARRAY_INIT_ELEM },
    { "any.convert_extern", C_ANY_CONVERT },
    { "extern.convert_any", C_EXTERN_CONVERT },
    { "atomic.fence", C_NOP },
    { "memory.atomic.notify", C_NOTIFY },
    { "memory.atomic.wait32", C_WAIT32 },
    { "memory.atomic.wait64", C_WAIT64 },
};

struct OpSig {
    Code code;
    u8 nin, nout;
    u8 in[4];
    u8 out[2];
    u8 vt;       // the value's or the lane's type
    bool atomic; // a memory access that must be aligned naturally
};

constexpr u32 MAX_OPS = 1024;
OpSig g_sig[MAX_OPS];
bool g_ready = false;

// A number or vector type's name, or a shape's; VBOT for none.
u8 kind_named(Str s)
{
    constexpr Str NAMES[] = { "i32", "i64", "f32", "f64", "v128" };
    for (u8 k = 0; k < 5; k++)
        if (s == NAMES[k])
            return k;
    if (s == "i8x16" || s == "i16x8" || s == "i32x4" || s == "i64x2" || s == "f32x4" ||
        s == "f64x2")
        return VV128;
    return VBOT;
}

// A shape's lane type.
u8 lane_kind(Str shape)
{
    if (shape == "i64x2")
        return VI64;
    if (shape == "f32x4")
        return VF32;
    if (shape == "f64x2")
        return VF64;
    return VI32;
}

bool one_of(Str s, const Str *set, u32 n)
{
    for (u32 i = 0; i < n; i++)
        if (s == set[i])
            return true;
    return false;
}

void sig(OpSig &g, const char *in, const char *out)
{
    auto kind = [](char c) -> u8 {
        switch (c) {
        case 'i':
            return VI32;
        case 'I':
            return VI64;
        case 'f':
            return VF32;
        case 'F':
            return VF64;
        default:
            return VV128;
        }
    };
    g.code = C_NUM;
    g.nin  = 0;
    for (const char *p = in; *p; p++)
        g.in[g.nin++] = kind(*p);
    g.nout = 0;
    for (const char *p = out; *p; p++)
        g.out[g.nout++] = kind(*p);
}

// Numbers and vectors: what they take and give, by their names.
void numeric(OpSig &g, Str pre, Str op)
{
    constexpr char K[] = { 'i', 'I', 'f', 'F', 'v' };
    char t             = K[kind_named(pre)];
    char one[2] = { t, 0 }, two[3] = { t, t, 0 }, three[4] = { t, t, t, 0 };
    if (kind_named(pre) != VV128) {
        constexpr Str REL[]   = { "eq",   "ne",   "lt_s", "lt_u", "gt_s", "gt_u", "le_s",
                                  "le_u", "ge_s", "ge_u", "lt",   "gt",   "le",   "ge" };
        constexpr Str UNARY[] = { "clz",       "ctz",        "popcnt",    "abs",     "neg",
                                  "ceil",      "floor",      "trunc",     "nearest", "sqrt",
                                  "extend8_s", "extend16_s", "extend32_s" };
        if (op == "eqz")
            sig(g, one, "i");
        else if (one_of(op, REL, 14))
            sig(g, two, "i");
        else if (one_of(op, UNARY, 13))
            sig(g, one, one);
        else if (op == "add128" || op == "sub128")
            sig(g, "IIII", "II");
        else if (op == "mul_wide_s" || op == "mul_wide_u")
            sig(g, "II", "II");
        else if (op.find("_i32") != Str::npos)
            sig(g, "i", one);
        else if (op.find("_i64") != Str::npos)
            sig(g, "I", one);
        else if (op.find("_f32") != Str::npos)
            sig(g, "f", one);
        else if (op.find("_f64") != Str::npos)
            sig(g, "F", one);
        else
            sig(g, two, one);
        return;
    }
    constexpr char L[]      = { 'i', 'I', 'f', 'F' };
    char lane[2]            = { L[lane_kind(pre)], 0 };
    constexpr Str TERNARY[] = { "bitselect", "relaxed_madd", "relaxed_nmadd", "relaxed_laneselect",
                                "relaxed_dot_i8x16_i7x16_add_s" };
    constexpr Str UNARY[]   = { "not",     "abs",      "neg",        "popcnt",
                                "sqrt",    "ceil",     "floor",      "trunc",
                                "nearest", "extend_",  "extadd_",    "convert_",
                                "demote_", "promote_", "trunc_sat_", "relaxed_trunc_" };
    bool unary              = false;
    for (Str u : UNARY)
        unary = unary || (u.ends_with("_") ? op.starts_with(u) : op == u);
    if (op == "splat")
        sig(g, lane, "v");
    else if (op == "any_true" || op == "all_true" || op == "bitmask")
        sig(g, "v", "i");
    else if (op == "shl" || op == "shr_s" || op == "shr_u")
        sig(g, "vi", "v");
    else if (one_of(op, TERNARY, 5))
        sig(g, three, one);
    else if (unary)
        sig(g, one, one);
    else
        sig(g, two, one);
}

void make_table()
{
    u32 n = op_count() < MAX_OPS ? op_count() : MAX_OPS;
    for (u32 i = 0; i < n; i++) {
        const OpDef &d = op_def(u16(i));
        OpSig &g       = g_sig[i];
        g.code         = C_OTHER;
        g.atomic       = d.name.find("atomic") != Str::npos;
        for (const Named &k : NAMED)
            if (d.name == k.name)
                g.code = k.code;
        if (g.code != C_OTHER)
            continue;
        usize dot = d.name.find('.');
        Str pre   = dot == Str::npos ? d.name : d.name.substr(0, dot);
        Str op    = dot == Str::npos ? Str() : d.name.substr(dot + 1);
        g.vt      = kind_named(pre);
        if (d.imm == Imm::MEM) {
            g.code = d.name.find(".rmw") != Str::npos
                         ? (d.name.find("cmpxchg") != Str::npos ? C_CMPXCHG : C_RMW)
                     : d.name.find("store") != Str::npos ? C_STORE
                                                         : C_LOAD;
        } else if (d.imm == Imm::MEM_LANE) {
            g.code = d.name.find("store") != Str::npos ? C_STORE_LANE : C_LOAD_LANE;
        } else if (d.imm == Imm::LANE) {
            g.code = op.starts_with("replace") ? C_REPLACE : C_EXTRACT;
            g.vt   = lane_kind(pre);
        } else if (d.imm == Imm::NONE && g.vt != VBOT) {
            numeric(g, pre, op);
        }
    }
    g_ready = true;
}

// ------------------------------------------------------------ the checker

// A block under way: where its body is, its types, and its stack.
enum : u8 { K_FUNC, K_CONST, K_BLOCK, K_LOOP, K_IF, K_ELSE, K_TRY, K_CATCH, K_TRY_TABLE };

struct Ctl {
    const Instr *owner; // the block instruction; null for a body or constant
    u32 part;           // the owner's part that comes next
    List<Instr *> seq;
    u32 next;
    u8 kind;
    u32 in_at, in_n, out_at, out_n; // in `pool`
    u32 height;
    bool unreachable;
    u32 undo; // locals set in it, from here in `undo`
    Loc at;
};

struct Global {
    VT type;
    bool mut;
};

struct Validator {
    Str file;
    Arena &arena;
    Diag &diag;
    bool failed = false;

    // The module's index spaces, as far as checked.
    Vec<const TypeDef *> defs;
    Vec<u32> canon; // equal types have equal numbers
    HashMap<Str, u32> groups;
    u32 next_canon = 0;
    u32 ntypes     = 0;
    Vec<u32> funcs; // their types
    Vec<const TableType *> tables;
    Vec<const MemType *> memories;
    Vec<Global> globals;
    u32 nglobals = 0; // visible
    Vec<u32> tags;
    Vec<VT> elems;
    u32 ndatas = 0;
    Vec<bool> refs; // functions a ref.func may name

    // The function under way.
    Vec<VT> locals;
    Vec<bool> set;
    Vec<u32> undo;
    Vec<VT> results;
    Vec<VT> vals;
    Vec<Ctl> ctls;
    Vec<VT> pool;
    Vec<VT> scratch;
    Vec<VT> snap;
    Vec<u8> key;

    Validator(Str name, Arena &a, Diag &d) : file(name), arena(a), diag(d) {}

    // ---------------------------------------------------- errors

    bool fail(Loc at, Str msg)
    {
        if (failed)
            return false;
        failed = true;
        Out where;
        where.put(file).put(':').num(at.line).put(':').num(at.col);
        diag.error_at(where.str(), msg);
        return false;
    }

    bool fail(Loc at, const Out &m) { return fail(at, m.str()); }

    void oom()
    {
        if (!failed)
            diag.error("out of memory");
        failed = true;
    }

    template <class T>
    void add(Vec<T> &v, T x)
    {
        if (!v.push(move(x)))
            oom();
    }

    // "unknown what x", when x is not below n.
    bool known(Loc at, Str what, u32 x, u32 n)
    {
        if (x < n)
            return true;
        Out m;
        m.put("unknown ").put(what).put(' ').num(x);
        return fail(at, m);
    }

    // ---------------------------------------------------- types

    const CompType &comp(u32 x) { return defs[x]->type.body; }

    bool check_heap(u32 h, Loc at) { return h >= H_ABS || known(at, "type", h, ntypes); }

    bool check_val(VT t, Loc at) { return t.kind != VREF || check_heap(t.heap, at); }

    bool check_vals(const List<ValType> &l, Loc at)
    {
        for (const ValType &t : l)
            if (!check_val(val_of(t), at))
                return false;
        return true;
    }

    // Type x, a function type: "unknown type x" or "non-function type x".
    bool func_type(u32 x, Loc at)
    {
        if (!known(at, "type", x, ntypes))
            return false;
        if (comp(x).kind == CompType::Kind::FuncType)
            return true;
        Out m;
        m.put("non-function type ").num(x);
        return fail(at, m);
    }

    bool struct_type(u32 x, Loc at)
    {
        if (!known(at, "type", x, ntypes))
            return false;
        if (comp(x).kind == CompType::Kind::StructType)
            return true;
        Out m;
        m.put("non-structure type ").num(x);
        return fail(at, m);
    }

    bool array_type(u32 x, Loc at)
    {
        if (!known(at, "type", x, ntypes))
            return false;
        if (comp(x).kind == CompType::Kind::ArrayType)
            return true;
        Out m;
        m.put("non-array type ").num(x);
        return fail(at, m);
    }

    // Concrete a is b or declares it a supertype, as far as known.
    bool idx_sub(u32 a, u32 b)
    {
        for (u32 n = 0; n <= defs.size(); n++) {
            if (a >= canon.size() || b >= canon.size())
                return a == b;
            if (canon[a] == canon[b])
                return true;
            const SubType &s = defs[a]->type;
            if (s.supers.empty())
                return false;
            a = s.supers[0].value;
        }
        return false;
    }

    bool heap_sub(u32 a, u32 b)
    {
        if (a == H_BOT)
            return true;
        if (b == H_BOT)
            return false;
        if (a < H_ABS && b < H_ABS)
            return idx_sub(a, b);
        if (a < H_ABS) {
            if (a >= ntypes)
                return false;
            switch (comp(a).kind) {
            case CompType::Kind::StructType:
                return is_abs(b, AbsHeapType::HAny) || is_abs(b, AbsHeapType::HEq) ||
                       is_abs(b, AbsHeapType::HStruct);
            case CompType::Kind::ArrayType:
                return is_abs(b, AbsHeapType::HAny) || is_abs(b, AbsHeapType::HEq) ||
                       is_abs(b, AbsHeapType::HArray);
            default:
                return is_abs(b, AbsHeapType::HFunc);
            }
        }
        auto A = AbsHeapType(a - H_ABS);
        if (b < H_ABS) {
            if (b >= ntypes)
                return false;
            bool func = comp(b).kind == CompType::Kind::FuncType;
            return (A == AbsHeapType::HNone && !func) || (A == AbsHeapType::HNoFunc && func);
        }
        auto B = AbsHeapType(b - H_ABS);
        if (A == B)
            return true;
        switch (A) {
        case AbsHeapType::HEq:
            return B == AbsHeapType::HAny;
        case AbsHeapType::HI31:
        case AbsHeapType::HStruct:
        case AbsHeapType::HArray:
            return B == AbsHeapType::HAny || B == AbsHeapType::HEq;
        case AbsHeapType::HNone:
            return B == AbsHeapType::HAny || B == AbsHeapType::HEq || B == AbsHeapType::HI31 ||
                   B == AbsHeapType::HStruct || B == AbsHeapType::HArray;
        case AbsHeapType::HNoFunc:
            return B == AbsHeapType::HFunc;
        case AbsHeapType::HNoExn:
            return B == AbsHeapType::HExn;
        case AbsHeapType::HNoExtern:
            return B == AbsHeapType::HExtern;
        default:
            return false;
        }
    }

    bool sub(VT a, VT b)
    {
        if (a.kind == VBOT)
            return true;
        if (a.kind != b.kind)
            return false;
        if (a.kind != VREF)
            return true;
        return (!a.null || b.null) && heap_sub(a.heap, b.heap);
    }

    bool subs(const VT *a, u32 na, const VT *b, u32 nb)
    {
        if (na != nb)
            return false;
        for (u32 i = 0; i < na; i++)
            if (!sub(a[i], b[i]))
                return false;
        return true;
    }

    u32 top_of(u32 h)
    {
        if (h < H_ABS)
            return h < ntypes && comp(h).kind == CompType::Kind::FuncType
                       ? abs_heap(AbsHeapType::HFunc)
                       : abs_heap(AbsHeapType::HAny);
        switch (AbsHeapType(h - H_ABS)) {
        case AbsHeapType::HFunc:
        case AbsHeapType::HNoFunc:
            return abs_heap(AbsHeapType::HFunc);
        case AbsHeapType::HExn:
        case AbsHeapType::HNoExn:
            return abs_heap(AbsHeapType::HExn);
        case AbsHeapType::HExtern:
        case AbsHeapType::HNoExtern:
            return abs_heap(AbsHeapType::HExtern);
        default:
            return abs_heap(AbsHeapType::HAny);
        }
    }

    static bool defaultable(VT t) { return t.kind != VREF || t.null; }

    bool storage_sub(const StorageType &a, const StorageType &b)
    {
        if (a.kind != b.kind)
            return false;
        return a.kind != StorageType::Kind::Value || sub(val_of(a.type), val_of(b.type));
    }

    bool field_sub(const FieldType &a, const FieldType &b)
    {
        return a.mutable_ == b.mutable_ && storage_sub(a.storage, b.storage) &&
               (!a.mutable_ || storage_sub(b.storage, a.storage));
    }

    bool comp_sub(const CompType &a, const CompType &b)
    {
        if (a.kind != b.kind)
            return false;
        switch (a.kind) {
        case CompType::Kind::StructType:
            if (a.fields.size() < b.fields.size())
                return false;
            for (u32 i = 0; i < b.fields.size(); i++)
                if (!field_sub(a.fields[i].type, b.fields[i].type))
                    return false;
            return true;
        case CompType::Kind::ArrayType:
            return field_sub(a.field, b.field);
        default:
            if (a.params.size() != b.params.size() || a.results.size() != b.results.size())
                return false;
            for (u32 i = 0; i < a.params.size(); i++)
                if (!sub(val_of(b.params[i].type), val_of(a.params[i].type)))
                    return false;
            for (u32 i = 0; i < a.results.size(); i++)
                if (!sub(val_of(a.results[i]), val_of(b.results[i])))
                    return false;
            return true;
        }
    }

    // ---------------------------------------------------- rec groups

    void key_u32(u32 v)
    {
        for (u32 k = 0; k < 4; k++)
            add(key, u8(v >> (8 * k)));
    }

    // A reference to type x from the group at g: its place in the group, or
    // the number of its equivalence class.
    bool key_type(u32 x, u32 g, Loc at)
    {
        if (!known(at, "type", x, ntypes))
            return false;
        add(key, u8(x >= g ? 'R' : 'C'));
        key_u32(x >= g ? x - g : canon[x]);
        return true;
    }

    bool key_val(const ValType &t, u32 g, Loc at)
    {
        add(key, u8(t.kind));
        if (t.kind != ValType::Kind::Ref)
            return true;
        add(key, u8(t.type.nullable));
        if (t.type.heap.kind == HeapType::Kind::Abstract) {
            add(key, u8('A'));
            add(key, u8(t.type.heap.abs));
            return true;
        }
        return key_type(t.type.heap.index.value, g, at);
    }

    bool key_storage(const FieldType &f, u32 g, Loc at)
    {
        add(key, u8(f.mutable_));
        add(key, u8(f.storage.kind));
        return f.storage.kind != StorageType::Kind::Value || key_val(f.storage.type, g, at);
    }

    // The group of types from g on, its types numbered: equal groups, and
    // so equal types, get equal numbers.
    bool canonical(const List<TypeDef> &group, u32 g, Loc at)
    {
        key.clear();
        for (const TypeDef &t : group) {
            const SubType &s = t.type;
            add(key, u8(s.final));
            key_u32(s.supers.size());
            for (const Idx &x : s.supers)
                if (!key_type(x.value, g, at))
                    return false;
            const CompType &c = s.body;
            add(key, u8(c.kind));
            if (c.kind == CompType::Kind::FuncType) {
                key_u32(c.params.size());
                for (const Param &p : c.params)
                    if (!key_val(p.type, g, at))
                        return false;
                key_u32(c.results.size());
                for (const ValType &r : c.results)
                    if (!key_val(r, g, at))
                        return false;
            } else if (c.kind == CompType::Kind::StructType) {
                key_u32(c.fields.size());
                for (const Field &f : c.fields)
                    if (!key_storage(f.type, g, at))
                        return false;
            } else if (!key_storage(c.field, g, at)) {
                return false;
            }
        }
        Str k    = Str(reinterpret_cast<const char *>(key.data()), key.size());
        u32 base = 0;
        if (const u32 *b = groups.find(k)) {
            base = *b;
        } else {
            k    = arena.str(k);
            base = next_canon;
            next_canon += group.size();
            if (arena.failed() || !groups.insert(k, base))
                oom();
        }
        for (u32 i = 0; i < group.size(); i++)
            add(canon, base + i);
        return !failed;
    }

    bool rec_group(const Decl::Type *d)
    {
        u32 g = defs.size();
        for (const TypeDef &t : d->group)
            add(defs, &t);
        ntypes = defs.size();
        if (!canonical(d->group, g, d->loc))
            return false;
        for (const TypeDef &t : d->group) {
            const SubType &s = t.type;
            if (s.supers.size() > 1)
                return fail(d->loc, "multiple supertypes");
            const CompType &c = s.body;
            if (c.kind == CompType::Kind::FuncType) {
                for (const Param &p : c.params)
                    if (!check_val(val_of(p.type), d->loc))
                        return false;
                if (!check_vals(c.results, d->loc))
                    return false;
            }
        }
        for (u32 i = 0; i < d->group.size(); i++) {
            const SubType &s = d->group[i].type;
            u32 x            = g + i;
            for (const Idx &y : s.supers) {
                Out m;
                if (y.value >= x) {
                    m.put("forward use of type ").num(y.value).put(" in sub type definition");
                    return fail(d->loc, m);
                }
                if (defs[y.value]->type.final) {
                    m.put("sub type ").num(x).put(" has final super type ").num(y.value);
                    return fail(d->loc, m);
                }
                if (!comp_sub(s.body, comp(y.value))) {
                    m.put("sub type ").num(x).put(" does not match super type ").num(y.value);
                    return fail(d->loc, m);
                }
            }
        }
        return true;
    }

    // ---------------------------------------------------- module types

    bool limits(const Limits &l, u64 range, Str msg, Loc at)
    {
        if (l.min > range || (l.max.has && l.max.value > range))
            return fail(at, msg);
        if (l.max.has && l.min > l.max.value)
            return fail(at, "size minimum must not be greater than maximum");
        return true;
    }

    bool mem_type(const MemType &m, Loc at)
    {
        bool wide = m.addr == AddrType::Addr64;
        u64 page  = m.page_size.has ? m.page_size.value : 65536;
        if (page != 1 && page != 65536)
            return fail(at, "invalid custom page size");
        u64 range =
            page == 1 ? (wide ? ~u64(0) : u64(1) << 32) : (wide ? u64(1) << 48 : u64(1) << 16);
        Str msg = wide ? "memory size must be at most 2^48 pages (256 TiB) for i64"_s
                       : "memory size must be at most 2^16 pages (4 GiB) for i32"_s;
        if (!limits(m.limits, range, msg, at))
            return false;
        if (m.shared && !m.limits.max.has)
            return fail(at, "shared memory must have maximum");
        return true;
    }

    bool table_type(const TableType &t, Loc at)
    {
        if (!check_val(ref_of(t.elem), at))
            return false;
        bool wide = t.addr == AddrType::Addr64;
        return limits(t.limits, wide ? ~u64(0) : 0xffffffffu,
                      wide ? "table size must be at most 2^64-1 for i64"_s
                           : "table size must be at most 2^32-1 for i32"_s,
                      at);
    }

    bool tag_type(u32 x, Loc at)
    {
        if (!func_type(x, at))
            return false;
        if (!comp(x).results.empty())
            return fail(at, "non-empty tag result type");
        return true;
    }

    // ---------------------------------------------------- the stack

    Ctl &top() { return ctls.back(); }

    void push(VT t) { add(vals, t); }

    void push_n(const VT *t, u32 n)
    {
        for (u32 i = 0; i < n; i++)
            push(t[i]);
    }

    // The i-th value from the top of this block's stack; bottom past it.
    VT peek(u32 i)
    {
        u32 avail = vals.size() - top().height;
        return i < avail ? vals[vals.size() - 1 - i] : VT();
    }

    bool mismatch(Str what, const VT *want, u32 nwant, Str has, const VT *got, u32 ngot, Loc at)
    {
        Out m;
        m.put("type mismatch: ").put(what).put(" requires ");
        put_types(m, want, nwant);
        m.put(" but ").put(has).put(" has ");
        put_types(m, got, ngot);
        return fail(at, m);
    }

    // `want`, off the top of the stack, as the reference pops a list.
    bool pop(const VT *want, u32 n, Loc at)
    {
        Ctl &c    = top();
        u32 avail = vals.size() - c.height;
        u32 k     = n < avail ? n : avail;
        u32 pad   = c.unreachable ? n - k : 0;
        scratch.clear();
        for (u32 i = 0; i < pad; i++)
            add(scratch, VT());
        for (u32 i = vals.size() - k; i < vals.size(); i++)
            add(scratch, vals[i]);
        if (!subs(scratch.data(), scratch.size(), want, n))
            return mismatch("instruction", want, n, "stack", scratch.data(), scratch.size(), at);
        vals.resize(vals.size() - k);
        return true;
    }

    bool pop1(VT t, Loc at) { return pop(&t, 1, at); }

    bool pop2(VT a, VT b, Loc at)
    {
        VT t[] = { a, b };
        return pop(t, 2, at);
    }

    bool pop3(VT a, VT b, VT c, Loc at)
    {
        VT t[] = { a, b, c };
        return pop(t, 3, at);
    }

    // Pool entries from `at`, as a pointer; valid until the pool grows.
    const VT *types(u32 at) { return pool.data() + at; }

    void unreachable()
    {
        vals.resize(top().height);
        top().unreachable = true;
    }

    // The reference type on top: bottom when not known.
    bool peek_ref(u32 i, VT &r, Loc at)
    {
        VT t = peek(i);
        if (t.kind == VBOT) {
            r = ref(false, H_BOT);
            return true;
        }
        if (t.kind == VREF) {
            r = t;
            return true;
        }
        Out m;
        m.put("type mismatch: instruction requires reference type but stack has ");
        put_type(m, t);
        return fail(at, m);
    }

    // ---------------------------------------------------- labels

    u32 labels()
    {
        u32 n = ctls.size();
        return !ctls.empty() && ctls[0].kind == K_CONST ? n - 1 : n;
    }

    bool label(u32 l, Loc at) { return known(at, "label", l, labels()); }

    Ctl &label_ctl(u32 l) { return ctls[ctls.size() - 1 - l]; }

    // A label's types: pool index and count.
    void label_types(u32 l, u32 &at, u32 &n)
    {
        Ctl &c = label_ctl(l);
        at     = c.kind == K_LOOP ? c.in_at : c.out_at;
        n      = c.kind == K_LOOP ? c.in_n : c.out_n;
    }

    // A copy of label l's types at the end of the pool.
    u32 label_copy(u32 l, u32 &n)
    {
        u32 at = 0;
        label_types(l, at, n);
        u32 start = pool.size();
        for (u32 i = 0; i < n; i++)
            add(pool, pool[at + i]);
        return start;
    }

    // ---------------------------------------------------- locals

    bool local(u32 x, Loc at) { return known(at, "local", x, locals.size()); }

    void init(u32 x)
    {
        if (!set[x]) {
            set[x] = true;
            add(undo, x);
        }
    }

    void restore(u32 mark)
    {
        while (undo.size() > mark) {
            set[undo.back()] = false;
            undo.pop();
        }
    }

    // ---------------------------------------------------- blocks

    // A block type into the pool: its params from `in`, results after.
    bool block_type(const BlockType &b, u32 &in, u32 &nin, u32 &out, u32 &nout, Loc at)
    {
        in  = pool.size();
        nin = 0;
        if (b.kind == BlockType::Kind::Result) {
            VT t = val_of(b.result);
            if (!check_val(t, at))
                return false;
            add(pool, t);
            out  = in;
            nout = 1;
            return true;
        }
        if (b.kind == BlockType::Kind::Use) {
            u32 x = b.use.type.value.value;
            if (!func_type(x, at))
                return false;
            const CompType &f = comp(x);
            for (const Param &p : f.params)
                add(pool, val_of(p.type));
            nin = f.params.size();
            for (const ValType &r : f.results)
                add(pool, val_of(r));
            out  = in + nin;
            nout = f.results.size();
            return true;
        }
        out  = in;
        nout = 0;
        return true;
    }

    void enter(const Instr *owner, List<Instr *> body, u8 kind, u32 in, u32 nin, u32 out, u32 nout,
               Loc at)
    {
        Ctl c{ owner, 1, body, 0, kind, in, nin, out, nout, vals.size(), false, undo.size(), at };
        add(ctls, c);
        push_n(types(in), nin);
    }

    // The block's body is done: its results, and nothing else, are there.
    bool leave_body()
    {
        Ctl &c = top();
        snap.clear();
        for (u32 i = c.height; i < vals.size(); i++)
            add(snap, vals[i]);
        if (!pop(types(c.out_at), c.out_n, c.at))
            return false;
        if (vals.size() != top().height)
            return mismatch("block", types(top().out_at), top().out_n, "stack", snap.data(),
                            snap.size(), top().at);
        return true;
    }

    // The next part of the block's instruction, the part before done.
    void next_part()
    {
        Ctl &c          = top();
        const Instr *in = c.owner;
        u32 k           = c.part++;
        auto again      = [&](List<Instr *> body, u8 kind) {
            restore(c.undo);
            vals.resize(c.height);
            c.unreachable = false;
            c.seq         = body;
            c.next        = 0;
            c.kind        = kind;
        };
        if (auto *b = in->as<Instr::If>(); b && k == 1) {
            again(b->else_body, K_ELSE);
            push_n(types(c.in_at), c.in_n);
            return;
        }
        if (auto *t = in->as<Instr::Try>(); t && k - 1 < t->catches.size()) {
            const LegacyCatch &lc = t->catches[k - 1];
            again(lc.body, K_CATCH);
            if (lc.kind == LegacyCatch::Kind::LegacyCatch) {
                u32 x = lc.tag.value;
                if (!known(in->loc, "tag", x, tags.size()))
                    return;
                for (const Param &p : comp(tags[x]).params)
                    push(val_of(p.type));
            }
            return;
        }
        // Done: the block's results replace it.
        u32 out_at = c.out_at, out_n = c.out_n, in_at = c.in_at;
        restore(c.undo);
        vals.resize(c.height);
        scratch.clear();
        for (u32 i = 0; i < out_n; i++)
            add(scratch, pool[out_at + i]);
        ctls.pop();
        pool.resize(in_at);
        for (VT t : scratch)
            push(t);
        if (auto *d = in->as<Instr::TryDelegate>())
            label(d->label_out.value, in->loc);
    }

    // `body`, checked as a block of type in -> out, both in the pool.
    void block(List<Instr *> body, u8 kind, u32 in, u32 nin, u32 out, u32 nout, Loc at)
    {
        ctls.clear();
        vals.clear();
        enter(nullptr, body, kind, in, nin, out, nout, at);
        while (!failed && !ctls.empty()) {
            Ctl &c = top();
            if (c.next < c.seq.size()) {
                instr(c.seq[c.next++]);
                continue;
            }
            if (!leave_body())
                return;
            if (!top().owner) {
                ctls.pop();
                return;
            }
            next_part();
        }
    }

    // A block instruction's start: its params off the stack, its body next.
    void open(const Instr *in, const BlockType &bt, List<Instr *> body, u8 kind, bool cond)
    {
        u32 i = 0, ni = 0, o = 0, no = 0;
        if (!block_type(bt, i, ni, o, no, in->loc))
            return;
        u32 want = pool.size();
        for (u32 k = 0; k < ni; k++)
            add(pool, pool[i + k]);
        if (cond)
            add(pool, num(VI32));
        if (!pop(types(want), pool.size() - want, in->loc))
            return;
        pool.resize(want);
        enter(in, body, kind, i, ni, o, no, in->loc);
    }

    // ---------------------------------------------------- instructions

    bool call_types(u32 type, Loc at, u32 &in, u32 &nin, u32 &out, u32 &nout)
    {
        const CompType &f = comp(type);
        in                = pool.size();
        for (const Param &p : f.params)
            add(pool, val_of(p.type));
        nin = f.params.size();
        out = pool.size();
        for (const ValType &r : f.results)
            add(pool, val_of(r));
        nout = f.results.size();
        (void)at;
        return true;
    }

    // A call of `type`: `extra` after its params; a return call checks its
    // results against the function's.
    void call(u32 type, bool tail, const VT *extra, u32 nextra, Loc at)
    {
        u32 mark = pool.size();
        u32 in = 0, nin = 0, out = 0, nout = 0;
        call_types(type, at, in, nin, out, nout);
        if (tail && !subs(types(out), nout, results.data(), results.size())) {
            Out m;
            m.put("type mismatch: current function requires result type ");
            put_types(m, results.data(), results.size());
            m.put(" but callee returns ");
            put_types(m, types(out), nout);
            fail(at, m);
            pool.resize(mark);
            return;
        }
        u32 want = pool.size();
        for (u32 k = 0; k < nin; k++)
            add(pool, pool[in + k]);
        for (u32 k = 0; k < nextra; k++)
            add(pool, extra[k]);
        if (!pop(types(want), pool.size() - want, at))
            return;
        if (tail)
            unreachable();
        else
            push_n(types(out), nout);
        pool.resize(mark);
    }

    bool table(u32 x, Loc at) { return known(at, "table", x, tables.size()); }

    bool memory(u32 x, Loc at) { return known(at, "memory", x, memories.size()); }

    VT table_addr(u32 x) { return num(addr_kind(tables[x]->addr)); }

    VT memory_addr(u32 x) { return num(addr_kind(memories[x]->addr)); }

    bool elem(u32 x, Loc at) { return known(at, "elem segment", x, elems.size()); }

    bool data(u32 x, Loc at) { return known(at, "data segment", x, ndatas); }

    bool func(u32 x, Loc at) { return known(at, "function", x, funcs.size()); }

    // A memory access of `size` bytes.
    bool mem_op(const OpSig &g, const Idx &memory_, u64 offset, u64 align, u64 size, Loc at)
    {
        if (!memory(memory_.value, at))
            return false;
        if (g.atomic ? align != size : align > size)
            return fail(at, g.atomic ? "atomic alignment must be natural"_s
                                     : "alignment must not be larger than natural"_s);
        if (memories[memory_.value]->addr == AddrType::Addr32 && offset >> 32)
            return fail(at, "offset out of range");
        return true;
    }

    void plain(const Instr *in, Opcode op)
    {
        const OpSig &g = g_sig[op];
        Loc at         = in->loc;
        VT r;
        switch (g.code) {
        case C_NUM: {
            VT t[4];
            for (u32 i = 0; i < g.nin; i++)
                t[i] = num(g.in[i]);
            if (pop(t, g.nin, at))
                for (u32 i = 0; i < g.nout; i++)
                    push(num(g.out[i]));
            break;
        }
        case C_UNREACHABLE:
            unreachable();
            break;
        case C_NOP:
            break;
        case C_DROP:
            pop1(peek(0), at);
            break;
        case C_RETURN:
            if (pop(results.data(), results.size(), at))
                unreachable();
            break;
        case C_THROW_REF:
            if (pop1(ref(true, abs_heap(AbsHeapType::HExn)), at))
                unreachable();
            break;
        case C_REF_IS_NULL:
            if (peek_ref(0, r, at) && pop1(ref(true, r.heap), at))
                push(num(VI32));
            break;
        case C_REF_AS_NON_NULL:
            if (peek_ref(0, r, at) && pop1(ref(true, r.heap), at))
                push(ref(false, r.heap));
            break;
        case C_REF_EQ: {
            VT e = ref(true, abs_heap(AbsHeapType::HEq));
            if (pop2(e, e, at))
                push(num(VI32));
            break;
        }
        case C_ARRAY_LEN:
            if (pop1(ref(true, abs_heap(AbsHeapType::HArray)), at))
                push(num(VI32));
            break;
        case C_ANY_CONVERT:
        case C_EXTERN_CONVERT: {
            u32 from = abs_heap(g.code == C_ANY_CONVERT ? AbsHeapType::HExtern : AbsHeapType::HAny);
            u32 to   = abs_heap(g.code == C_ANY_CONVERT ? AbsHeapType::HAny : AbsHeapType::HExtern);
            if (peek_ref(0, r, at) && pop1(ref(r.null, from), at))
                push(ref(r.null, to));
            break;
        }
        case C_REF_I31:
            if (pop1(num(VI32), at))
                push(ref(false, abs_heap(AbsHeapType::HI31)));
            break;
        case C_I31_GET:
            if (pop1(ref(true, abs_heap(AbsHeapType::HI31)), at))
                push(num(VI32));
            break;
        default:
            fail(at, "unknown operator");
            break;
        }
    }

    void select(const Instr::Select *s)
    {
        Loc at = s->loc;
        VT t;
        if (s->typed) {
            if (s->results.size() != 1) {
                fail(at, "invalid result arity other than 1 is not (yet) allowed");
                return;
            }
            t = val_of(s->results[0]);
            if (!check_val(t, at))
                return;
        } else {
            t = peek(1);
            if (t.kind == VREF) {
                Out m;
                m.put("type mismatch: instruction requires numeric or vector type but stack has ");
                put_type(m, t);
                fail(at, m);
                return;
            }
        }
        if (pop3(t, t, num(VI32), at))
            push(t);
    }

    void br(const Instr::Br *b)
    {
        const OpSig &g = g_sig[b->op];
        Loc at         = b->loc;
        u32 l          = b->label.value;
        if (!label(l, at))
            return;
        u32 mark = pool.size(), n = 0;
        u32 lt = label_copy(l, n);
        switch (g.code) {
        case C_RETHROW:
            if (label_ctl(l).kind != K_CATCH) {
                fail(at, "invalid rethrow label");
                break;
            }
            unreachable();
            break;
        case C_BR:
            if (pop(types(lt), n, at))
                unreachable();
            break;
        case C_BR_IF:
            add(pool, num(VI32));
            if (pop(types(lt), n + 1, at))
                push_n(types(lt), n);
            break;
        case C_BR_ON_NULL: {
            VT r;
            if (!peek_ref(0, r, at))
                break;
            add(pool, ref(true, r.heap));
            if (pop(types(lt), n + 1, at)) {
                push_n(types(lt), n);
                push(ref(false, r.heap));
            }
            break;
        }
        default: { // br_on_non_null
            VT last = n ? pool[lt + n - 1] : VT();
            if (!n || last.kind != VREF) {
                Out m;
                m.put("type mismatch: instruction requires reference type but label has ");
                put_types(m, types(lt), n);
                fail(at, m);
                break;
            }
            pool[lt + n - 1] = ref(true, last.heap);
            if (pop(types(lt), n, at))
                push_n(types(lt), n - 1);
            break;
        }
        }
        pool.resize(mark);
    }

    void br_table(const Instr::BrTable *b)
    {
        Loc at = b->loc;
        u32 d  = b->default_.value;
        if (!label(d, at))
            return;
        u32 mark = pool.size(), n = 0, la = 0;
        label_types(d, la, n);
        u32 ts = pool.size();
        for (u32 i = 0; i < n; i++)
            add(pool, peek(n - i));
        auto match = [&](u32 l) {
            if (!label(l, at))
                return false;
            u32 a = 0, k = 0;
            label_types(l, a, k);
            if (!subs(types(ts), n, types(a), k))
                return mismatch("instruction", types(a), k, "stack", types(ts), n, at);
            return true;
        };
        bool ok = match(d);
        for (const Idx &x : b->labels)
            ok = ok && match(x.value);
        if (ok) {
            add(pool, num(VI32));
            if (pop(types(ts), n + 1, at))
                unreachable();
        }
        pool.resize(mark);
    }

    // A cast's reference type, checked.
    bool ref_type(const RefType &r, VT &t, Loc at)
    {
        t = ref_of(r);
        return check_val(t, at);
    }

    void br_on_cast(const Instr::BrOnCast *b)
    {
        Loc at     = b->loc;
        bool fail_ = g_sig[b->op].code == C_BR_ON_CAST_FAIL;
        VT rt1, rt2;
        if (!label(b->label.value, at) || !ref_type(b->from, rt1, at) || !ref_type(b->to, rt2, at))
            return;
        if (!sub(rt2, rt1)) {
            Out m;
            m.put("type mismatch on cast: type ");
            put_type(m, rt2);
            m.put(" does not match ");
            put_type(m, rt1);
            fail(at, m);
            return;
        }
        VT diff     = ref(rt2.null ? false : rt1.null, rt1.heap);
        VT to_label = fail_ ? diff : rt2;
        u32 mark = pool.size(), n = 0;
        u32 lt = label_copy(b->label.value, n);
        if (!n || !sub(to_label, pool[lt + n - 1])) {
            Out m;
            m.put("type mismatch: instruction requires type ");
            put_type(m, to_label);
            m.put(" but label has ");
            put_types(m, types(lt), n);
            fail(at, m);
            pool.resize(mark);
            return;
        }
        pool[lt + n - 1] = rt1;
        if (pop(types(lt), n, at)) {
            push_n(types(lt), n - 1);
            push(fail_ ? rt2 : diff);
        }
        pool.resize(mark);
    }

    void try_table(const Instr::TryTable *t)
    {
        Loc at = t->loc;
        u32 i = 0, ni = 0, o = 0, no = 0;
        u32 mark = pool.size();
        if (!block_type(t->type, i, ni, o, no, at))
            return;
        pool.resize(mark);
        for (const Catch &c : t->catches) {
            u32 want = pool.size();
            if (c.kind == Catch::Kind::Catch || c.kind == Catch::Kind::CatchRef) {
                if (!known(at, "tag", c.tag.value, tags.size()))
                    return;
                for (const Param &p : comp(tags[c.tag.value]).params)
                    add(pool, val_of(p.type));
            }
            if (c.kind == Catch::Kind::CatchRef || c.kind == Catch::Kind::CatchAllRef)
                add(pool, ref(false, abs_heap(AbsHeapType::HExn)));
            if (!label(c.label.value, at))
                return;
            u32 la = 0, n = 0;
            label_types(c.label.value, la, n);
            if (!subs(types(want), pool.size() - want, types(la), n)) {
                mismatch("catch handler", types(la), n, "label", types(want), pool.size() - want,
                         at);
                return;
            }
            pool.resize(want);
        }
        open(t, t->type, t->body, K_TRY_TABLE, false);
    }

    void instr(const Instr *in)
    {
        Loc at = in->loc;
        switch (in->kind) {
        case Instr::Kind::Plain:
            plain(in, in->as<Instr::Plain>()->op);
            break;
        case Instr::Kind::Select:
            select(in->as<Instr::Select>());
            break;
        case Instr::Kind::Block: {
            auto *b = in->as<Instr::Block>();
            open(in, b->type, b->body, op_def(b->op).name == "loop" ? K_LOOP : K_BLOCK, false);
            break;
        }
        case Instr::Kind::If: {
            auto *b = in->as<Instr::If>();
            open(in, b->type, b->then_body, K_IF, true);
            break;
        }
        case Instr::Kind::TryTable:
            try_table(in->as<Instr::TryTable>());
            break;
        case Instr::Kind::Try: {
            auto *b = in->as<Instr::Try>();
            open(in, b->type, b->body, K_TRY, false);
            break;
        }
        case Instr::Kind::TryDelegate: {
            auto *b = in->as<Instr::TryDelegate>();
            open(in, b->type, b->body, K_TRY, false);
            break;
        }
        case Instr::Kind::Br:
            br(in->as<Instr::Br>());
            break;
        case Instr::Kind::BrTable:
            br_table(in->as<Instr::BrTable>());
            break;
        case Instr::Kind::BrOnCast:
            br_on_cast(in->as<Instr::BrOnCast>());
            break;
        case Instr::Kind::Index:
            index(in->as<Instr::Index>());
            break;
        case Instr::Kind::Index2:
            index2(in->as<Instr::Index2>());
            break;
        case Instr::Kind::ArrayNewFixed: {
            auto *a = in->as<Instr::ArrayNewFixed>();
            u32 x   = a->type.value;
            if (!array_type(x, at))
                return;
            VT t     = unpacked(comp(x).field.storage);
            u32 mark = pool.size();
            for (u32 i = 0; i < a->count && !failed; i++)
                add(pool, t);
            if (pop(types(mark), a->count, at))
                push(ref(false, x));
            pool.resize(mark);
            break;
        }
        case Instr::Kind::CallIndirect: {
            auto *c = in->as<Instr::CallIndirect>();
            u32 x   = c->table.value;
            u32 y   = c->type.type.value.value;
            if (!table(x, at) || !func_type(y, at))
                return;
            VT e = ref_of(tables[x]->elem);
            if (!sub(e, ref(true, abs_heap(AbsHeapType::HFunc)))) {
                Out m;
                m.put(
                    "type mismatch: instruction requires table of function type but table has "
                    "element type ");
                put_type(m, e);
                fail(at, m);
                return;
            }
            VT a = table_addr(x);
            call(y, g_sig[c->op].code == C_RETURN_CALL_INDIRECT, &a, 1, at);
            break;
        }
        case Instr::Kind::MemArg:
            mem_arg(in->as<Instr::MemArg>());
            break;
        case Instr::Kind::MemArgLane: {
            auto *m        = in->as<Instr::MemArgLane>();
            const OpSig &g = g_sig[m->op];
            u32 bytes      = 1u << op_def(m->op).align;
            if (!mem_op(g, m->memory, m->offset, m->align, bytes, at))
                return;
            if (m->lane >= 16 / bytes) {
                fail(at, "invalid lane index");
                return;
            }
            if (pop2(memory_addr(m->memory.value), num(VV128), at) && g.code == C_LOAD_LANE)
                push(num(VV128));
            break;
        }
        case Instr::Kind::I32Const:
            push(num(VI32));
            break;
        case Instr::Kind::I64Const:
            push(num(VI64));
            break;
        case Instr::Kind::F32Const:
            push(num(VF32));
            break;
        case Instr::Kind::F64Const:
            push(num(VF64));
            break;
        case Instr::Kind::V128Const:
            push(num(VV128));
            break;
        case Instr::Kind::Lane: {
            auto *l        = in->as<Instr::Lane>();
            const OpSig &g = g_sig[l->op];
            Str shape      = op_def(l->op).name.substr(0, op_def(l->op).name.find('.'));
            u32 lanes      = shape == "i8x16"               ? 16
                             : shape == "i16x8"             ? 8
                             : g.vt == VI32 || g.vt == VF32 ? 4
                                                            : 2;
            if (l->lane >= lanes) {
                fail(at, "invalid lane index");
                return;
            }
            if (g.code == C_EXTRACT) {
                if (pop1(num(VV128), at))
                    push(num(g.vt));
            } else if (pop2(num(VV128), num(g.vt), at)) {
                push(num(VV128));
            }
            break;
        }
        case Instr::Kind::Shuffle:
            for (u8 l : in->as<Instr::Shuffle>()->lanes)
                if (l >= 32) {
                    fail(at, "invalid lane index");
                    return;
                }
            if (pop2(num(VV128), num(VV128), at))
                push(num(VV128));
            break;
        case Instr::Kind::RefNull: {
            const HeapType &h = in->as<Instr::RefNull>()->type;
            u32 heap = h.kind == HeapType::Kind::Abstract ? abs_heap(h.abs) : h.index.value;
            if (check_heap(heap, at))
                push(ref(true, heap));
            break;
        }
        case Instr::Kind::RefTypeOp: {
            auto *r = in->as<Instr::RefTypeOp>();
            VT t;
            if (!ref_type(r->type, t, at))
                return;
            if (!pop1(ref(true, top_of(t.heap)), at))
                return;
            push(g_sig[r->op].code == C_REF_TEST ? num(VI32) : t);
            break;
        }
        }
    }

    void mem_arg(const Instr::MemArg *m)
    {
        const OpSig &g = g_sig[m->op];
        const OpDef &d = op_def(m->op);
        Loc at         = m->loc;
        if (!mem_op(g, m->memory, m->offset, m->align, u64(1) << d.align, at))
            return;
        VT a = memory_addr(m->memory.value);
        VT t = num(g.vt);
        switch (g.code) {
        case C_LOAD:
            if (pop1(a, at))
                push(t);
            break;
        case C_STORE:
            pop2(a, t, at);
            break;
        case C_RMW:
            if (pop2(a, t, at))
                push(t);
            break;
        case C_CMPXCHG:
            if (pop3(a, t, t, at))
                push(t);
            break;
        case C_NOTIFY:
            if (pop2(a, num(VI32), at))
                push(num(VI32));
            break;
        default: // wait
            if (pop3(a, num(g.code == C_WAIT32 ? VI32 : VI64), num(VI64), at))
                push(num(VI32));
            break;
        }
    }

    void index(const Instr::Index *x)
    {
        const OpSig &g = g_sig[x->op];
        Loc at         = x->loc;
        u32 i          = x->x.value;
        switch (g.code) {
        case C_CALL:
        case C_RETURN_CALL:
            if (func(i, at))
                call(funcs[i], g.code == C_RETURN_CALL, nullptr, 0, at);
            break;
        case C_CALL_REF:
        case C_RETURN_CALL_REF:
            if (func_type(i, at)) {
                VT r = ref(true, i);
                call(i, g.code == C_RETURN_CALL_REF, &r, 1, at);
            }
            break;
        case C_THROW: {
            if (!known(at, "tag", i, tags.size()))
                break;
            u32 mark = pool.size();
            for (const Param &p : comp(tags[i]).params)
                add(pool, val_of(p.type));
            if (pop(types(mark), pool.size() - mark, at))
                unreachable();
            pool.resize(mark);
            break;
        }
        case C_LOCAL_GET:
            if (!local(i, at))
                break;
            if (!set[i]) {
                fail(at, "uninitialized local");
                break;
            }
            push(locals[i]);
            break;
        case C_LOCAL_SET:
            if (local(i, at) && pop1(locals[i], at))
                init(i);
            break;
        case C_LOCAL_TEE:
            if (local(i, at) && pop1(locals[i], at)) {
                init(i);
                push(locals[i]);
            }
            break;
        case C_GLOBAL_GET:
            if (known(at, "global", i, nglobals))
                push(globals[i].type);
            break;
        case C_GLOBAL_SET:
            if (!known(at, "global", i, nglobals))
                break;
            if (!globals[i].mut) {
                fail(at, "immutable global");
                break;
            }
            pop1(globals[i].type, at);
            break;
        case C_TABLE_GET:
            if (table(i, at) && pop1(table_addr(i), at))
                push(ref_of(tables[i]->elem));
            break;
        case C_TABLE_SET:
            if (table(i, at))
                pop2(table_addr(i), ref_of(tables[i]->elem), at);
            break;
        case C_TABLE_SIZE:
            if (table(i, at))
                push(table_addr(i));
            break;
        case C_TABLE_GROW:
            if (table(i, at) && pop2(ref_of(tables[i]->elem), table_addr(i), at))
                push(table_addr(i));
            break;
        case C_TABLE_FILL:
            if (table(i, at))
                pop3(table_addr(i), ref_of(tables[i]->elem), table_addr(i), at);
            break;
        case C_ELEM_DROP:
            elem(i, at);
            break;
        case C_MEMORY_SIZE:
            if (memory(i, at))
                push(memory_addr(i));
            break;
        case C_MEMORY_GROW:
            if (memory(i, at) && pop1(memory_addr(i), at))
                push(memory_addr(i));
            break;
        case C_MEMORY_FILL:
            if (memory(i, at))
                pop3(memory_addr(i), num(VI32), memory_addr(i), at);
            break;
        case C_DATA_DROP:
            data(i, at);
            break;
        case C_REF_FUNC:
            if (!func(i, at))
                break;
            if (!refs[i]) {
                Out m;
                m.put("undeclared function reference ").num(i);
                fail(at, m);
                break;
            }
            push(ref(false, funcs[i]));
            break;
        case C_STRUCT_NEW:
        case C_STRUCT_NEW_DEFAULT: {
            if (!struct_type(i, at))
                break;
            u32 mark = pool.size();
            for (const Field &f : comp(i).fields) {
                VT t = unpacked(f.type.storage);
                if (g.code == C_STRUCT_NEW_DEFAULT && !defaultable(t)) {
                    fail(at, "field type is not defaultable");
                    break;
                }
                add(pool, t);
            }
            if (g.code == C_STRUCT_NEW_DEFAULT)
                pool.resize(mark);
            if (!failed && pop(types(mark), pool.size() - mark, at))
                push(ref(false, i));
            pool.resize(mark);
            break;
        }
        case C_ARRAY_NEW:
        case C_ARRAY_NEW_DEFAULT: {
            if (!array_type(i, at))
                break;
            VT t = unpacked(comp(i).field.storage);
            if (g.code == C_ARRAY_NEW_DEFAULT) {
                if (!defaultable(t)) {
                    fail(at, "array type is not defaultable");
                    break;
                }
                if (pop1(num(VI32), at))
                    push(ref(false, i));
            } else if (pop2(t, num(VI32), at)) {
                push(ref(false, i));
            }
            break;
        }
        case C_ARRAY_GET:
        case C_ARRAY_GET_PACKED: {
            if (!array_type(i, at))
                break;
            const StorageType &s = comp(i).field.storage;
            bool packed          = s.kind != StorageType::Kind::Value;
            if (packed != (g.code == C_ARRAY_GET_PACKED)) {
                fail(at, packed ? "array is packed"_s : "array is unpacked"_s);
                break;
            }
            if (pop2(ref(true, i), num(VI32), at))
                push(unpacked(s));
            break;
        }
        case C_ARRAY_SET:
        case C_ARRAY_FILL: {
            if (!array_type(i, at))
                break;
            const FieldType &f = comp(i).field;
            if (!f.mutable_) {
                fail(at, "immutable array");
                break;
            }
            VT t = unpacked(f.storage);
            if (g.code == C_ARRAY_SET) {
                pop3(ref(true, i), num(VI32), t, at);
            } else {
                VT w[] = { ref(true, i), num(VI32), t, num(VI32) };
                pop(w, 4, at);
            }
            break;
        }
        default:
            fail(at, "unknown operator");
            break;
        }
    }

    void index2(const Instr::Index2 *x)
    {
        const OpSig &g = g_sig[x->op];
        Loc at         = x->loc;
        u32 a = x->x.value, b = x->y.value;
        switch (g.code) {
        case C_STRUCT_GET:
        case C_STRUCT_GET_PACKED:
        case C_STRUCT_SET: {
            if (!struct_type(a, at))
                break;
            const CompType &c = comp(a);
            if (b >= c.fields.size()) {
                Out m;
                m.put("unknown field ").num(b);
                fail(at, m);
                break;
            }
            const FieldType &f = c.fields[b].type;
            bool packed        = f.storage.kind != StorageType::Kind::Value;
            if (g.code == C_STRUCT_SET) {
                if (!f.mutable_) {
                    fail(at, "immutable field");
                    break;
                }
                pop2(ref(true, a), unpacked(f.storage), at);
                break;
            }
            if (packed != (g.code == C_STRUCT_GET_PACKED)) {
                fail(at, packed ? "field is packed"_s : "field is unpacked"_s);
                break;
            }
            if (pop1(ref(true, a), at))
                push(unpacked(f.storage));
            break;
        }
        case C_ARRAY_NEW_DATA:
        case C_ARRAY_INIT_DATA: {
            if (!array_type(a, at))
                break;
            const FieldType &f = comp(a).field;
            if (g.code == C_ARRAY_INIT_DATA && !f.mutable_) {
                fail(at, "immutable array");
                break;
            }
            if (!data(b, at))
                break;
            VT t = unpacked(f.storage);
            if (t.kind == VREF) {
                fail(at, "array type is not numeric or vector");
                break;
            }
            if (g.code == C_ARRAY_NEW_DATA) {
                if (pop2(num(VI32), num(VI32), at))
                    push(ref(false, a));
            } else {
                VT w[] = { ref(true, a), num(VI32), num(VI32), num(VI32) };
                pop(w, 4, at);
            }
            break;
        }
        case C_ARRAY_NEW_ELEM:
        case C_ARRAY_INIT_ELEM: {
            if (!array_type(a, at))
                break;
            const FieldType &f = comp(a).field;
            if (g.code == C_ARRAY_INIT_ELEM && !f.mutable_) {
                fail(at, "immutable array");
                break;
            }
            if (!elem(b, at))
                break;
            if (!sub(elems[b], unpacked(f.storage))) {
                Out m;
                m.put("type mismatch: element segment's type ");
                put_type(m, elems[b]);
                m.put(" does not match array's field type");
                fail(at, m);
                break;
            }
            if (g.code == C_ARRAY_NEW_ELEM) {
                if (pop2(num(VI32), num(VI32), at))
                    push(ref(false, a));
            } else {
                VT w[] = { ref(true, a), num(VI32), num(VI32), num(VI32) };
                pop(w, 4, at);
            }
            break;
        }
        case C_ARRAY_COPY: {
            if (!array_type(a, at) || !array_type(b, at))
                break;
            const FieldType &d = comp(a).field, &s = comp(b).field;
            if (!d.mutable_) {
                fail(at, "immutable array");
                break;
            }
            if (!storage_sub(s.storage, d.storage)) {
                fail(at, "array types do not match");
                break;
            }
            VT w[] = { ref(true, a), num(VI32), ref(true, b), num(VI32), num(VI32) };
            pop(w, 5, at);
            break;
        }
        case C_MEMORY_COPY: {
            if (!memory(a, at) || !memory(b, at))
                break;
            VT x1 = memory_addr(a), x2 = memory_addr(b);
            pop3(x1, x2, x1.kind == VI32 || x2.kind == VI32 ? num(VI32) : num(VI64), at);
            break;
        }
        case C_TABLE_COPY: {
            if (!table(a, at) || !table(b, at))
                break;
            VT t1 = ref_of(tables[a]->elem), t2 = ref_of(tables[b]->elem);
            if (!sub(t2, t1)) {
                Out m;
                m.put("type mismatch: source element type ");
                put_type(m, t2);
                m.put(" does not match destination element type ");
                put_type(m, t1);
                fail(at, m);
                break;
            }
            VT x1 = table_addr(a), x2 = table_addr(b);
            pop3(x1, x2, x1.kind == VI32 || x2.kind == VI32 ? num(VI32) : num(VI64), at);
            break;
        }
        case C_MEMORY_INIT:
            if (memory(a, at) && data(b, at))
                pop3(memory_addr(a), num(VI32), num(VI32), at);
            break;
        case C_TABLE_INIT: {
            if (!table(a, at) || !elem(b, at))
                break;
            VT t1 = ref_of(tables[a]->elem);
            if (!sub(elems[b], t1)) {
                Out m;
                m.put("type mismatch: element segment's type ");
                put_type(m, elems[b]);
                m.put(" does not match table's element type ");
                put_type(m, t1);
                fail(at, m);
                break;
            }
            pop3(table_addr(a), num(VI32), num(VI32), at);
            break;
        }
        default:
            fail(at, "unknown operator");
            break;
        }
    }

    // ---------------------------------------------------- constants

    bool is_const(const Instr *in)
    {
        switch (in->kind) {
        case Instr::Kind::I32Const:
        case Instr::Kind::I64Const:
        case Instr::Kind::F32Const:
        case Instr::Kind::F64Const:
        case Instr::Kind::V128Const:
        case Instr::Kind::RefNull:
        case Instr::Kind::ArrayNewFixed:
            return true;
        case Instr::Kind::Plain: {
            Opcode op = in->as<Instr::Plain>()->op;
            Str n     = op_def(op).name;
            Code c    = g_sig[op].code;
            return n == "i32.add" || n == "i32.sub" || n == "i32.mul" || n == "i64.add" ||
                   n == "i64.sub" || n == "i64.mul" || c == C_REF_I31 || c == C_ANY_CONVERT ||
                   c == C_EXTERN_CONVERT;
        }
        case Instr::Kind::Index: {
            auto *x = in->as<Instr::Index>();
            switch (g_sig[x->op].code) {
            case C_REF_FUNC:
            case C_STRUCT_NEW:
            case C_STRUCT_NEW_DEFAULT:
            case C_ARRAY_NEW:
            case C_ARRAY_NEW_DEFAULT:
                return true;
            case C_GLOBAL_GET:
                return known(in->loc, "global", x->x.value, nglobals) && !globals[x->x.value].mut;
            default:
                return false;
            }
        }
        default:
            return false;
        }
    }

    // A constant expression of type t.
    bool constant(const List<Instr *> &l, VT t, Loc at)
    {
        for (const Instr *in : l)
            if (!is_const(in))
                return failed ? false : fail(in->loc, "constant expression required");
        locals.clear();
        set.clear();
        undo.clear();
        results.clear();
        pool.clear();
        add(pool, t);
        block(l, K_CONST, 0, 0, 0, 1, at);
        return !failed;
    }

    // ---------------------------------------------------- the module

    // Functions a ref.func may name: those named outside function bodies.
    void declared(const Module &m)
    {
        if (!refs.resize(funcs.size()))
            oom();
        auto mark = [&](u32 x) {
            if (x < refs.size())
                refs[x] = true;
        };
        auto in = [&](const List<Instr *> &l) {
            for (const Instr *i : l)
                if (auto *x = i->as<Instr::Index>(); x && g_sig[x->op].code == C_REF_FUNC)
                    mark(x->x.value);
        };
        for (const Decl *d : m.decls) {
            if (auto *e = d->as<Decl::Elem>()) {
                for (const Idx &x : e->elems.funcs)
                    mark(x.value);
                for (const Expr &x : e->elems.items)
                    in(x.instrs);
                if (e->mode.kind == ElemMode::Kind::ElemActive)
                    in(e->mode.offset);
            } else if (auto *g = d->as<Decl::Global>()) {
                in(g->init);
            } else if (auto *t = d->as<Decl::Table>()) {
                in(t->init);
            } else if (auto *a = d->as<Decl::Data>()) {
                if (a->mode.kind == DataMode::Kind::DataActive)
                    in(a->mode.offset);
            } else if (auto *x = d->as<Decl::Export>()) {
                if (x->sort == ExternKind::FuncKind)
                    mark(x->index.value);
            }
        }
    }

    bool import(const Decl::Import *i)
    {
        const Extern &x = i->desc;
        Loc at          = i->loc;
        switch (x.kind) {
        case Extern::Kind::FuncImport:
            if (!func_type(x.type.type.value.value, at))
                return false;
            add(funcs, x.type.type.value.value);
            return true;
        case Extern::Kind::TableImport:
            if (!table_type(x.table, at))
                return false;
            add(tables, &x.table);
            return true;
        case Extern::Kind::MemoryImport:
            if (!mem_type(x.memory, at))
                return false;
            add(memories, &x.memory);
            return true;
        case Extern::Kind::GlobalImport: {
            VT t = val_of(x.global.type);
            if (!check_val(t, at))
                return false;
            add(globals, Global{ t, x.global.mutable_ });
            nglobals = globals.size();
            return true;
        }
        default:
            if (!tag_type(x.type.type.value.value, at))
                return false;
            add(tags, x.type.type.value.value);
            return true;
        }
    }

    bool body(const Decl::Func *f)
    {
        u32 x = f->type.type.value.value;
        locals.clear();
        set.clear();
        undo.clear();
        results.clear();
        for (const Param &p : comp(x).params) {
            add(locals, val_of(p.type));
            add(set, true);
        }
        for (const Local &l : f->locals) {
            VT t = val_of(l.type);
            if (!check_val(t, f->loc))
                return false;
            add(locals, t);
            add(set, defaultable(t));
        }
        for (const ValType &r : comp(x).results)
            add(results, val_of(r));
        pool.clear();
        for (VT t : results)
            add(pool, t);
        block(f->body, K_FUNC, 0, 0, 0, results.size(), f->loc);
        return !failed;
    }

    bool module(const Module &m)
    {
        if (!g_ready)
            make_table();
        for (const Decl *d : m.decls)
            if (auto *t = d->as<Decl::Type>(); t && !rec_group(t))
                return false;
        for (const Decl *d : m.decls)
            if (auto *i = d->as<Decl::Import>(); i && !import(i))
                return false;
        u32 imported_globals = globals.size();
        for (const Decl *d : m.decls)
            if (auto *t = d->as<Decl::Tag>()) {
                if (!tag_type(t->type.type.value.value, d->loc))
                    return false;
                add(tags, t->type.type.value.value);
            }
        for (const Decl *d : m.decls)
            if (auto *f = d->as<Decl::Func>()) {
                if (!func_type(f->type.type.value.value, d->loc))
                    return false;
                add(funcs, f->type.type.value.value);
            }
        declared(m);
        for (const Decl *d : m.decls)
            if (auto *x = d->as<Decl::Memory>()) {
                if (!mem_type(x->type, d->loc))
                    return false;
                add(memories, &x->type);
            }
        nglobals = imported_globals;
        for (const Decl *d : m.decls)
            if (auto *t = d->as<Decl::Table>()) {
                if (!table_type(t->type, d->loc))
                    return false;
                VT e = ref_of(t->type.elem);
                if (t->init.empty() && !e.null) {
                    Out msg;
                    msg.put("type mismatch: block requires [");
                    put_type(msg, e);
                    msg.put("] but stack has [");
                    put_type(msg, ref(true, e.heap));
                    msg.put(']');
                    return fail(d->loc, msg);
                }
                if (!t->init.empty() && !constant(t->init, e, d->loc))
                    return false;
                add(tables, &t->type);
            }
        for (const Decl *d : m.decls)
            if (auto *g = d->as<Decl::Global>()) {
                VT t = val_of(g->type.type);
                if (!check_val(t, d->loc) || !constant(g->init, t, d->loc))
                    return false;
                add(globals, Global{ t, g->type.mutable_ });
                nglobals = globals.size();
            }
        for (const Decl *d : m.decls)
            if (auto *a = d->as<Decl::Data>()) {
                if (a->mode.kind == DataMode::Kind::DataActive) {
                    u32 x = a->mode.memory.value;
                    if (!memory(x, d->loc) || !constant(a->mode.offset, memory_addr(x), d->loc))
                        return false;
                }
                ndatas++;
            }
        for (const Decl *d : m.decls)
            if (auto *e = d->as<Decl::Elem>(); e && !elem_segment(e))
                return false;
        for (const Decl *d : m.decls)
            if (auto *f = d->as<Decl::Func>(); f && !body(f))
                return false;
        for (const Decl *d : m.decls)
            if (auto *s = d->as<Decl::Start>()) {
                u32 x = s->func.value;
                if (!func(x, d->loc))
                    return false;
                const CompType &f = comp(funcs[x]);
                if (!f.params.empty() || !f.results.empty())
                    return fail(d->loc, "start function must not have parameters or results");
            }
        return exports(m);
    }

    bool elem_segment(const Decl::Elem *e)
    {
        Loc at            = e->loc;
        const ElemList &l = e->elems;
        VT rt = l.kind == ElemList::Kind::Funcs ? ref(false, abs_heap(AbsHeapType::HFunc))
                                                : ref_of(l.type);
        if (!check_val(rt, at))
            return false;
        for (const Idx &x : l.funcs)
            if (!func(x.value, x.loc))
                return false;
        for (const Expr &x : l.items)
            if (!constant(x.instrs, rt, at))
                return false;
        if (e->mode.kind == ElemMode::Kind::ElemActive) {
            u32 x = e->mode.table.value;
            if (!table(x, at))
                return false;
            VT t = ref_of(tables[x]->elem);
            if (!sub(rt, t)) {
                Out m;
                m.put("type mismatch: element segment's type ");
                put_type(m, rt);
                m.put(" does not match table's element type ");
                put_type(m, t);
                return fail(at, m);
            }
            if (!constant(e->mode.offset, table_addr(x), at))
                return false;
        }
        add(elems, rt);
        return true;
    }

    bool exports(const Module &m)
    {
        HashMap<Str, u32> names;
        for (const Decl *d : m.decls) {
            auto *x = d->as<Decl::Export>();
            if (!x)
                continue;
            u32 i = x->index.value;
            bool ok;
            switch (x->sort) {
            case ExternKind::FuncKind:
                ok = func(i, d->loc);
                break;
            case ExternKind::TableKind:
                ok = table(i, d->loc);
                break;
            case ExternKind::MemoryKind:
                ok = memory(i, d->loc);
                break;
            case ExternKind::GlobalKind:
                ok = known(d->loc, "global", i, globals.size());
                break;
            default:
                ok = known(d->loc, "tag", i, tags.size());
                break;
            }
            if (!ok)
                return false;
            if (names.contains(x->name)) {
                Out msg;
                msg.put("duplicate export name \"").put(x->name).put('"');
                return fail(d->loc, msg);
            }
            if (!names.insert(x->name, 0))
                oom();
        }
        return !failed;
    }
};

} // namespace

bool validate(Str name, Arena &arena, const Module &m, Diag &diag)
{
    Validator v(name, arena, diag);
    return v.module(m) && !v.failed;
}
