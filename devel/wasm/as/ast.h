// The syntax tree of wat.asdl, by hand; test/asdl.mjs checks that the two
// agree. The mapping:
//
//  * A product is a struct of its fields.
//  * A sum of constructors without fields is an enum class.
//  * Any other sum is a struct with a tag, `Kind kind`, and every
//    constructor's fields side by side, held by value.
//  * Decl and Instr, which are many and differ widely in size, are nodes
//    instead: a base with the tag and the attributes, and one struct per
//    constructor derived from it, `Instr::Block`. They are held by pointer.
//  * T* is List<T>, T? is Opt<T>, a location is Loc. identifier, name and
//    bytes are Str; opcode is Opcode.
//  * A field that is a C++ keyword gets a trailing underscore.
//
// Everything is trivially destructible and lives in an Arena, which frees
// it all at once.
#pragma once

#include "kernel/str.h"
#include "kernel/traits.h"
#include "kernel/types.h"
#include "kernel/vec.h"

struct Out;

namespace wat {

// ------------------------------------------------------------ primitives

// Line and column; the file is the tree's.
struct Loc {
    u32 line = 0;
    u32 col  = 0;
};

// An instruction: its place in lib/optable.h's table.
using Opcode = u16;

template <class T>
struct Opt {
    bool has = false;
    T value{};
};

// A sequence in the arena.
template <class T>
struct List {
    T *p  = nullptr;
    u32 n = 0;

    u32 size() const { return n; }

    bool empty() const { return n == 0; }

    const T &operator[](u32 i) const { return p[i]; }

    const T *begin() const { return p; }

    const T *end() const { return p + n; }
};

// Blocks freed at once. A failed allocation returns null and is sticky.
struct Arena {
    Arena()                         = default;
    Arena(const Arena &)            = delete;
    Arena &operator=(const Arena &) = delete;
    ~Arena();

    bool failed() const { return oom; }

    template <class T>
    T *make()
    {
        static_assert(is_trivially_destructible<T>);
        void *p = alloc(sizeof(T), alignof(T));
        return p ? new (p) T() : nullptr;
    }

    // A Decl or Instr node, its tag and location set.
    template <class T>
    T *node(Loc loc)
    {
        T *n = make<T>();
        if (n) {
            n->kind = T::KIND;
            n->loc  = loc;
        }
        return n;
    }

    template <class T>
    List<T> list(const Vec<T> &v)
    {
        static_assert(is_trivially_destructible<T>);
        List<T> l;
        if (v.empty())
            return l;
        l.p = static_cast<T *>(alloc(v.size() * sizeof(T), alignof(T)));
        if (!l.p)
            return List<T>();
        for (usize i = 0; i < v.size(); i++)
            new (l.p + i) T(v[i]);
        l.n = u32(v.size());
        return l;
    }

    Str str(Str s);

private:
    struct Block {
        Block *next;
    };

    void *alloc(usize n, usize align);

    Block *blocks = nullptr;
    u8 *at        = nullptr;
    u8 *end       = nullptr;
    bool oom      = false;
};

struct Decl;
struct Instr;

// ------------------------------------------------------------ enumerations

enum class ExternKind : u8 { FuncKind, TableKind, MemoryKind, GlobalKind, TagKind };

enum class Section : u8 {
    TypeSec,
    ImportSec,
    FuncSec,
    TableSec,
    MemorySec,
    TagSec,
    GlobalSec,
    ExportSec,
    StartSec,
    ElemSec,
    CodeSec,
    DataSec,
    DataCountSec
};

enum class AbsHeapType : u8 {
    HAny,
    HEq,
    HI31,
    HStruct,
    HArray,
    HNone,
    HFunc,
    HNoFunc,
    HExn,
    HNoExn,
    HExtern,
    HNoExtern
};

enum class AddrType : u8 { Addr32, Addr64 };

enum class BranchHint : u8 { Unlikely, Likely };

// ------------------------------------------------------------ names

struct Bind {
    Opt<Str> id;
    Opt<Str> name;
};

struct Idx {
    enum class Kind : u8 { Num, Id };
    Kind kind = Kind::Num;
    u32 value = 0;
    Str id;
    Loc loc;
};

// ------------------------------------------------------------ types

struct HeapType {
    enum class Kind : u8 { Abstract, Concrete };
    Kind kind       = Kind::Abstract;
    AbsHeapType abs = AbsHeapType::HAny;
    Idx index;
};

struct RefType {
    bool nullable = false;
    HeapType heap;
};

struct ValType {
    enum class Kind : u8 { I32, I64, F32, F64, V128, Ref };
    Kind kind = Kind::I32;
    RefType type;
};

struct StorageType {
    enum class Kind : u8 { Value, I8, I16 };
    Kind kind = Kind::Value;
    ValType type;
};

struct FieldType {
    StorageType storage;
    bool mutable_ = false;
};

struct Param {
    Bind bind;
    ValType type;
};

struct Field {
    Bind bind;
    FieldType type;
};

struct CompType {
    enum class Kind : u8 { FuncType, StructType, ArrayType };
    Kind kind = Kind::FuncType;
    List<Param> params;
    List<ValType> results;
    List<Field> fields;
    FieldType field;
};

struct SubType {
    bool final = true;
    List<Idx> supers;
    CompType body;
};

struct TypeDef {
    Bind bind;
    SubType type;
};

struct Limits {
    u64 min = 0;
    Opt<u64> max;
};

struct MemType {
    AddrType addr = AddrType::Addr32;
    Limits limits;
    bool shared = false;
    Opt<u64> page_size;
};

struct TableType {
    AddrType addr = AddrType::Addr32;
    Limits limits;
    RefType elem;
};

struct GlobalType {
    ValType type;
    bool mutable_ = false;
};

struct TypeUse {
    Opt<Idx> type;
    List<Param> params;
    List<ValType> results;
};

struct BlockType {
    enum class Kind : u8 { NoResult, Result, Use };
    Kind kind = Kind::NoResult;
    ValType result;
    TypeUse use;
};

// ------------------------------------------------------------ module parts

struct Extern {
    enum class Kind : u8 { FuncImport, TableImport, MemoryImport, GlobalImport, TagImport };
    Kind kind = Kind::FuncImport;
    Bind bind;
    TypeUse type;
    TableType table;
    MemType memory;
    GlobalType global;
};

struct Local {
    Bind bind;
    ValType type;
};

struct ElemMode {
    enum class Kind : u8 { ElemPassive, ElemActive, ElemDeclare };
    Kind kind = Kind::ElemPassive;
    Idx table;
    List<Instr *> offset;
};

struct Expr {
    List<Instr *> instrs;
};

struct ElemList {
    enum class Kind : u8 { Funcs, Exprs };
    Kind kind = Kind::Funcs;
    List<Idx> funcs;
    RefType type;
    List<Expr> items;
};

struct DataMode {
    enum class Kind : u8 { DataPassive, DataActive };
    Kind kind = Kind::DataPassive;
    Idx memory;
    List<Instr *> offset;
};

struct Place {
    enum class Kind : u8 { BeforeFirst, AfterLast, Before, After };
    Kind kind       = Kind::AfterLast;
    Section section = Section::TypeSec;
};

struct Catch {
    enum class Kind : u8 { Catch, CatchRef, CatchAll, CatchAllRef };
    Kind kind = Kind::Catch;
    Idx tag;
    Idx label;
};

struct LegacyCatch {
    enum class Kind : u8 { LegacyCatch, LegacyCatchAll };
    Kind kind = Kind::LegacyCatch;
    Idx tag;
    List<Instr *> body;
};

// ------------------------------------------------------------ nodes

struct Decl {
    enum class Kind : u8 {
        Type,
        Import,
        Func,
        Table,
        Memory,
        Global,
        Tag,
        Export,
        Start,
        Elem,
        Data,
        Custom
    };
    struct Type;
    struct Import;
    struct Func;
    struct Table;
    struct Memory;
    struct Global;
    struct Tag;
    struct Export;
    struct Start;
    struct Elem;
    struct Data;
    struct Custom;

    Kind kind = Kind::Type;
    Loc loc;

    // This node as constructor T, or null.
    template <class T>
    const T *as() const
    {
        return kind == T::KIND ? static_cast<const T *>(this) : nullptr;
    }
};

struct Decl::Type : Decl {
    static constexpr Kind KIND = Kind::Type;
    List<TypeDef> group;
};

struct Decl::Import : Decl {
    static constexpr Kind KIND = Kind::Import;
    Str module;
    Str item;
    Extern desc;
    List<Str> exports;
};

struct Decl::Func : Decl {
    static constexpr Kind KIND = Kind::Func;
    Bind bind;
    List<Str> exports;
    TypeUse type;
    List<Local> locals;
    List<Instr *> body;
};

struct Decl::Table : Decl {
    static constexpr Kind KIND = Kind::Table;
    Bind bind;
    List<Str> exports;
    TableType type;
    List<Instr *> init;
    Opt<ElemList> elems;
};

struct Decl::Memory : Decl {
    static constexpr Kind KIND = Kind::Memory;
    Bind bind;
    List<Str> exports;
    MemType type;
    Opt<Str> data;
};

struct Decl::Global : Decl {
    static constexpr Kind KIND = Kind::Global;
    Bind bind;
    List<Str> exports;
    GlobalType type;
    List<Instr *> init;
};

struct Decl::Tag : Decl {
    static constexpr Kind KIND = Kind::Tag;
    Bind bind;
    List<Str> exports;
    TypeUse type;
};

struct Decl::Export : Decl {
    static constexpr Kind KIND = Kind::Export;
    Str name;
    ExternKind sort = ExternKind::FuncKind;
    Idx index;
};

struct Decl::Start : Decl {
    static constexpr Kind KIND = Kind::Start;
    Idx func;
};

struct Decl::Elem : Decl {
    static constexpr Kind KIND = Kind::Elem;
    Bind bind;
    ElemMode mode;
    ElemList elems;
};

struct Decl::Data : Decl {
    static constexpr Kind KIND = Kind::Data;
    Bind bind;
    DataMode mode;
    Str init;
};

struct Decl::Custom : Decl {
    static constexpr Kind KIND = Kind::Custom;
    Str name;
    Place place;
    Str content;
};

struct Instr {
    enum class Kind : u8 {
        Plain,
        Select,
        Block,
        If,
        TryTable,
        Try,
        TryDelegate,
        Br,
        BrTable,
        BrOnCast,
        Index,
        Index2,
        ArrayNewFixed,
        CallIndirect,
        MemArg,
        MemArgLane,
        I32Const,
        I64Const,
        F32Const,
        F64Const,
        V128Const,
        Lane,
        Shuffle,
        RefNull,
        RefTypeOp
    };
    struct Plain;
    struct Select;
    struct Block;
    struct If;
    struct TryTable;
    struct Try;
    struct TryDelegate;
    struct Br;
    struct BrTable;
    struct BrOnCast;
    struct Index;
    struct Index2;
    struct ArrayNewFixed;
    struct CallIndirect;
    struct MemArg;
    struct MemArgLane;
    struct I32Const;
    struct I64Const;
    struct F32Const;
    struct F64Const;
    struct V128Const;
    struct Lane;
    struct Shuffle;
    struct RefNull;
    struct RefTypeOp;

    Kind kind = Kind::Plain;
    Loc loc;

    // This node as constructor T, or null.
    template <class T>
    const T *as() const
    {
        return kind == T::KIND ? static_cast<const T *>(this) : nullptr;
    }
};

struct Instr::Plain : Instr {
    static constexpr Kind KIND = Kind::Plain;
    Opcode op                  = 0;
};

struct Instr::Select : Instr {
    static constexpr Kind KIND = Kind::Select;
    bool typed                 = false;
    List<ValType> results;
};

struct Instr::Block : Instr {
    static constexpr Kind KIND = Kind::Block;
    Opcode op                  = 0;
    Bind label;
    BlockType type;
    List<Instr *> body;
};

struct Instr::If : Instr {
    static constexpr Kind KIND = Kind::If;
    Bind label;
    BlockType type;
    List<Instr *> then_body;
    bool has_else = false;
    List<Instr *> else_body;
    Opt<BranchHint> hint;
};

struct Instr::TryTable : Instr {
    static constexpr Kind KIND = Kind::TryTable;
    Bind label;
    BlockType type;
    List<Catch> catches;
    List<Instr *> body;
};

struct Instr::Try : Instr {
    static constexpr Kind KIND = Kind::Try;
    Bind label;
    BlockType type;
    List<Instr *> body;
    List<LegacyCatch> catches;
};

struct Instr::TryDelegate : Instr {
    static constexpr Kind KIND = Kind::TryDelegate;
    Bind label;
    BlockType type;
    List<Instr *> body;
    Idx label_out;
};

struct Instr::Br : Instr {
    static constexpr Kind KIND = Kind::Br;
    Opcode op                  = 0;
    Idx label;
    Opt<BranchHint> hint;
};

struct Instr::BrTable : Instr {
    static constexpr Kind KIND = Kind::BrTable;
    List<Idx> labels;
    Idx default_;
};

struct Instr::BrOnCast : Instr {
    static constexpr Kind KIND = Kind::BrOnCast;
    Opcode op                  = 0;
    Idx label;
    RefType from;
    RefType to;
};

struct Instr::Index : Instr {
    static constexpr Kind KIND = Kind::Index;
    Opcode op                  = 0;
    Idx x;
};

struct Instr::Index2 : Instr {
    static constexpr Kind KIND = Kind::Index2;
    Opcode op                  = 0;
    Idx x;
    Idx y;
};

struct Instr::ArrayNewFixed : Instr {
    static constexpr Kind KIND = Kind::ArrayNewFixed;
    Idx type;
    u32 count = 0;
};

struct Instr::CallIndirect : Instr {
    static constexpr Kind KIND = Kind::CallIndirect;
    Opcode op                  = 0;
    Idx table;
    TypeUse type;
};

struct Instr::MemArg : Instr {
    static constexpr Kind KIND = Kind::MemArg;
    Opcode op                  = 0;
    Idx memory;
    u64 offset = 0;
    u64 align  = 0;
};

struct Instr::MemArgLane : Instr {
    static constexpr Kind KIND = Kind::MemArgLane;
    Opcode op                  = 0;
    Idx memory;
    u64 offset = 0;
    u64 align  = 0;
    u8 lane    = 0;
};

struct Instr::I32Const : Instr {
    static constexpr Kind KIND = Kind::I32Const;
    u32 value                  = 0;
};

struct Instr::I64Const : Instr {
    static constexpr Kind KIND = Kind::I64Const;
    u64 value                  = 0;
};

struct Instr::F32Const : Instr {
    static constexpr Kind KIND = Kind::F32Const;
    u32 bits                   = 0;
};

struct Instr::F64Const : Instr {
    static constexpr Kind KIND = Kind::F64Const;
    u64 bits                   = 0;
};

struct Instr::V128Const : Instr {
    static constexpr Kind KIND = Kind::V128Const;
    Str value;
};

struct Instr::Lane : Instr {
    static constexpr Kind KIND = Kind::Lane;
    Opcode op                  = 0;
    u8 lane                    = 0;
};

struct Instr::Shuffle : Instr {
    static constexpr Kind KIND = Kind::Shuffle;
    List<u8> lanes;
};

struct Instr::RefNull : Instr {
    static constexpr Kind KIND = Kind::RefNull;
    HeapType type;
};

struct Instr::RefTypeOp : Instr {
    static constexpr Kind KIND = Kind::RefTypeOp;
    Opcode op                  = 0;
    RefType type;
};

// ------------------------------------------------------------ the module

struct Module {
    Bind bind;
    List<Decl *> decls;
};

// The tree as S-expressions, in one canonical form (see ast.cpp). An
// opcode is printed by `op_name`; locations only with `locs`.
void print(Out &out, const Module &m, Str (*op_name)(Opcode), bool locs);

} // namespace wat
