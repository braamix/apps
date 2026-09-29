#include "parser.h"

#include "lexer.h"
#include "number.h"
#include "optable.h"

using namespace wat;

namespace {

// The keywords that name no instruction. The script's own are among them:
// the reference knows them, so they are unexpected rather than unknown.
constexpr Str WORDS[] = {
    "i8",
    "i16",
    "i32",
    "i64",
    "f32",
    "f64",
    "v128",
    "i8x16",
    "i16x8",
    "i32x4",
    "i64x2",
    "f32x4",
    "f64x2",
    "any",
    "eq",
    "i31",
    "struct",
    "array",
    "none",
    "func",
    "nofunc",
    "exn",
    "noexn",
    "extern",
    "noextern",
    "anyref",
    "eqref",
    "i31ref",
    "structref",
    "arrayref",
    "nullref",
    "funcref",
    "nullfuncref",
    "exnref",
    "nullexnref",
    "externref",
    "nullexternref",
    "ref",
    "null",
    "mut",
    "field",
    "sub",
    "final",
    "rec",
    "then",
    "else",
    "end",
    "catch",
    "catch_ref",
    "catch_all",
    "catch_all_ref",
    "delegate",
    "do",
    "type",
    "param",
    "result",
    "local",
    "global",
    "memory",
    "table",
    "tag",
    "data",
    "elem",
    "declare",
    "offset",
    "item",
    "start",
    "import",
    "export",
    "module",
    "shared",
    "pagesize",
    "binary",
    "quote",
    "definition",
    "instance",
    "script",
    "register",
    "invoke",
    "get",
    "assert_malformed",
    "assert_invalid",
    "assert_malformed_custom",
    "assert_invalid_custom",
    "assert_unlinkable",
    "assert_return",
    "assert_trap",
    "assert_exception",
    "assert_exhaustion",
    "nan:canonical",
    "nan:arithmetic",
    "either",
    "input",
    "output",
    "ref.struct",
    "ref.array",
    "ref.exn",
    "ref.extern",
    "ref.host",
};

// In the order of AbsHeapType.
constexpr Str ABS[]  = { "any",  "eq",     "i31", "struct", "array",  "none",
                         "func", "nofunc", "exn", "noexn",  "extern", "noextern" };
constexpr Str REFS[] = { "anyref",   "eqref",      "i31ref",    "structref",
                         "arrayref", "nullref",    "funcref",   "nullfuncref",
                         "exnref",   "nullexnref", "externref", "nullexternref" };

// In the order of ValType's first constructors.
constexpr Str NUMS[] = { "i32", "i64", "f32", "f64", "v128" };

constexpr Str SHAPES[]   = { "i8x16", "i16x8", "i32x4", "i64x2", "f32x4", "f64x2" };
constexpr u32 LANES[]    = { 16, 8, 4, 2, 4, 2 };
constexpr u32 LANE_LOG[] = { 0, 1, 2, 3, 2, 3 };

// In the order of Section.
constexpr Str SECTIONS[] = { "type",   "import", "func", "table", "memory", "tag",      "global",
                             "export", "start",  "elem", "code",  "data",   "datacount" };

// Where an allocation failed, so that a node can still be filled in.
alignas(8) u8 g_scratch[512];

bool is_word(Str s)
{
    for (Str w : WORDS)
        if (w == s)
            return true;
    return false;
}

// A keyword the language has: an instruction, another word, or a memory
// operand with its number.
bool known(Str s)
{
    u16 op;
    if (find_op(s, op) || is_word(s))
        return true;
    if (s.starts_with("offset="))
        return is_nat(s.substr(7));
    if (s.starts_with("align="))
        return is_nat(s.substr(6));
    return false;
}

// Frames of the instruction stack.
enum class F : u8 {
    Root,   // the expression being parsed
    Plain,  // (op …), gathering its operands
    Flat,   // block … end, and loop, if, try_table, try alike
    Folded, // (block …), and loop, if, try_table, try alike
};

// Where an if or a try is.
enum Phase : u8 {
    COND,       // (if: folded operands
    THEN,       // then, flat or folded
    AFTER_THEN, // (then …) seen
    ELSE,       // else, flat or folded
    AFTER_ELSE, // (else …) seen
    DO_NEXT,    // (try: (do next
    DO,         // try's body
    AFTER_DO,   // (do …) or a (catch …) seen
    CATCH,      // a catch's body
    DONE,       // (delegate …) seen
};

struct Frame {
    F kind         = F::Root;
    Imm imm        = Imm::NONE;
    u8 phase       = 0;
    bool all       = false; // a catch_all seen
    bool delegated = false;
    bool has_else  = false;
    Token at; // the keyword
    Opcode op    = 0;
    Instr *plain = nullptr;
    Bind label;
    BlockType type;
    List<Catch> catches;
    Opt<BranchHint> hint;
    List<Instr *> first; // then, or try's body
    List<Instr *> else_body;
    Idx label_out;
    Vec<Instr *> seq;  // the sequence under way
    Vec<Instr *> cond; // a folded if's operands
    Vec<LegacyCatch> legacy;
    LegacyCatch cur;
};

// What an import or a definition was, for the rules on order.
enum Kind : u8 { K_IMPORT, K_FUNC, K_TABLE, K_MEMORY, K_GLOBAL, K_TAG, K_START };

constexpr Str KIND_NAME[] = { "import", "function", "table", "memory", "global", "tag", "start" };

struct Seen {
    Kind kind;
    Token at;
};

struct Parser {
    Str file;
    Str src;
    Lexer lx;
    Arena &arena;
    Diag &diag;
    bool failed = false;

    Vec<Token> ahead; // lexed and not taken, annotations among them
    u32 head    = 0;
    bool hinted = false; // a branch hint stood right before the last token taken
    Token hint;
    Vec<u8> bytes; // scratch
    Vec<Frame> stack;
    bool one_done = false;
    Vec<Decl *> decls;
    Vec<Seen> seen;
    Token field_at; // the '(' of the field under way

    Parser(Str name, Str source, Arena &a, Diag &d)
        : file(name), src(source), lx(source), arena(a), diag(d)
    {
    }

    // ---------------------------------------------------- errors

    void fail(const Token &t, Str msg)
    {
        if (failed)
            return;
        failed = true;
        Out where;
        where.put(file).put(':').num(t.line).put(':').num(t.col);
        diag.error_at(where.str(), msg);
    }

    void unexpected(const Token &t) { fail(t, "unexpected token"); }

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

    template <class T>
    List<T> list(const Vec<T> &v)
    {
        List<T> l = arena.list(v);
        if (arena.failed())
            oom();
        return l;
    }

    template <class T>
    T *node(Loc at)
    {
        T *n = arena.node<T>(at);
        if (!n) {
            static_assert(sizeof(T) <= sizeof g_scratch);
            oom();
            n = new (g_scratch) T();
        }
        return n;
    }

    // ---------------------------------------------------- tokens

    Str text(const Token &t) const { return src.substr(t.at, t.len); }

    static Loc loc(const Token &t) { return Loc{ t.line, t.col }; }

    // One more token into `ahead`. An error, lexical or an unknown keyword,
    // is raised as the token is read, as the reference raises it.
    void lex()
    {
        Token t = lx.next();
        if (t.kind == Tok::Error) {
            Token at;
            at.line = lx.err_line;
            at.col  = lx.err_col;
            Out m;
            m.put(lx.msg);
            if (!lx.what.empty())
                m.put(' ').put(lx.what);
            fail(at, m.str());
            return;
        }
        if (t.kind == Tok::Keyword && !known(text(t))) {
            Out m;
            m.put("unknown operator ").put(text(t));
            fail(t, m.str());
            return;
        }
        add(ahead, t);
    }

    // The k-th token ahead that is not an annotation; Eof once failed.
    Token peek(u32 k = 0)
    {
        u32 i = head, n = 0;
        for (;;) {
            while (!failed && i >= ahead.size())
                lex();
            if (failed)
                return Token();
            if (ahead[i].kind != Tok::Annot && n++ == k)
                return ahead[i];
            i++;
        }
    }

    // The next token that is not an annotation. The annotations before it
    // are white space, but a branch hint is kept for an if or a br_if.
    Token take()
    {
        Token t = peek();
        if (failed)
            return t;
        hinted = false;
        for (; ahead[head].kind == Tok::Annot; head++)
            if (annot_id(ahead[head]) == "metadata.code.branch_hint") {
                hinted = true;
                hint   = ahead[head];
            }
        head++;
        if (head == ahead.size()) {
            ahead.clear();
            head = 0;
        }
        return t;
    }

    bool is(Tok k, u32 i = 0) { return peek(i).kind == k; }

    bool is_kw(Str w, u32 i = 0)
    {
        Token t = peek(i);
        return t.kind == Tok::Keyword && text(t) == w;
    }

    // '(' w
    bool open_kw(Str w) { return is(Tok::LParen) && is_kw(w, 1); }

    // '(' w, taken.
    void enter()
    {
        take();
        take();
    }

    void want(Tok k)
    {
        Token t = take();
        if (t.kind != k)
            unexpected(t);
    }

    void close() { want(Tok::RParen); }

    template <u32 N>
    i32 keyword_in(const Str (&words)[N], const Token &t)
    {
        if (t.kind != Tok::Keyword)
            return -1;
        Str s = text(t);
        for (u32 i = 0; i < N; i++)
            if (words[i] == s)
                return i32(i);
        return -1;
    }

    // ---------------------------------------------------- annotations

    Str annot_id(const Token &a)
    {
        bytes.clear();
        if (!token_bytes(src, a, bytes))
            oom();
        return Str(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }

    // The annotation `id` among those right ahead, taken.
    bool annot(Str id, Token &a)
    {
        peek();
        for (u32 i = head; !failed && ahead[i].kind == Tok::Annot; i++)
            if (annot_id(ahead[i]) == id) {
                a = ahead[i];
                ahead.erase(i);
                return true;
            }
        return false;
    }

    // The tokens inside annotation `a`, after its id.
    void annot_items(const Token &a, Vec<Token> &items)
    {
        Lexer x(src);
        x.pos  = a.at + 2;
        x.line = a.line;
        x.bol  = a.at + 1 - a.col;
        x.next();
        for (u32 depth = 0;;) {
            Token t = x.next();
            if (t.kind == Tok::Eof || t.kind == Tok::Error || (t.kind == Tok::RParen && !depth))
                return;
            depth += t.kind == Tok::LParen;
            depth -= t.kind == Tok::RParen;
            add(items, t);
        }
    }

    // (@name "…") right ahead, into `b`.
    void annot_name(Bind &b)
    {
        Token a;
        if (!annot("name", a))
            return;
        Vec<Token> items;
        annot_items(a, items);
        if (items.size() != 1 || items[0].kind != Tok::String) {
            fail(a, "malformed @name annotation");
            return;
        }
        b.name.has   = true;
        b.name.value = name(items[0]);
    }

    Opt<BranchHint> branch_hint(bool has, const Token &a)
    {
        Opt<BranchHint> h;
        if (!has)
            return h;
        Vec<Token> items;
        annot_items(a, items);
        Str s;
        if (items.size() == 1 && items[0].kind == Tok::String)
            s = string(items[0]);
        if (s.size() != 1 || u8(s[0]) > 1) {
            fail(a, "malformed branch hint");
            return h;
        }
        h.has   = true;
        h.value = s[0] ? BranchHint::Likely : BranchHint::Unlikely;
        return h;
    }

    // (@custom name place? string*)
    void custom(const Token &a)
    {
        Vec<Token> t;
        annot_items(a, t);
        auto *c = node<Decl::Custom>(loc(a));
        u32 i   = 0;
        if (t.size() < 1 || t[0].kind != Tok::String) {
            fail(a, "malformed @custom annotation");
            return;
        }
        c->name = name(t[i++]);
        if (i < t.size() && t[i].kind == Tok::LParen) {
            bool ok   = i + 3 < t.size() && t[i + 1].kind == Tok::Keyword &&
                        t[i + 2].kind == Tok::Keyword && t[i + 3].kind == Tok::RParen;
            Str where = ok ? text(t[i + 1]) : Str();
            Str what  = ok ? text(t[i + 2]) : Str();
            i32 s     = ok ? keyword_in(SECTIONS, t[i + 2]) : -1;
            if (where == "before" && what == "first")
                c->place.kind = Place::Kind::BeforeFirst;
            else if (where == "after" && what == "last")
                c->place.kind = Place::Kind::AfterLast;
            else if ((where == "before" || where == "after") && s >= 0) {
                c->place.kind    = where == "before" ? Place::Kind::Before : Place::Kind::After;
                c->place.section = Section(s);
            } else {
                fail(a, "malformed @custom annotation");
                return;
            }
            i += 4;
        }
        Vec<u8> content;
        for (; i < t.size(); i++) {
            if (t[i].kind != Tok::String) {
                fail(a, "malformed @custom annotation");
                return;
            }
            Str s = string(t[i]);
            for (char ch : s)
                add(content, u8(ch));
        }
        c->content = arena.str(Str(reinterpret_cast<const char *>(content.data()), content.size()));
        add(decls, static_cast<Decl *>(c));
    }

    void customs()
    {
        Token a;
        while (!failed && annot("custom", a))
            custom(a);
    }

    // ---------------------------------------------------- values

    // A String's bytes: a slice of the source, or decoded into the arena.
    Str string(const Token &t)
    {
        Str lit = text(t);
        if (lit.find('\\') == Str::npos)
            return lit.substr(1, lit.size() - 2);
        bytes.clear();
        if (!token_bytes(src, t, bytes))
            oom();
        Str s = arena.str(Str(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
        if (arena.failed())
            oom();
        return s;
    }

    // A String that is a name.
    Str name(const Token &t)
    {
        if (t.kind != Tok::String) {
            unexpected(t);
            return Str();
        }
        Str s = string(t);
        if (!is_utf8(s))
            fail(t, "malformed UTF-8 encoding");
        return s;
    }

    // string*, one after another.
    Str strings()
    {
        Vec<u8> all;
        while (!failed && is(Tok::String)) {
            Str s = string(take());
            for (char c : s)
                add(all, u8(c));
        }
        Str s = arena.str(Str(reinterpret_cast<const char *>(all.data()), all.size()));
        if (arena.failed())
            oom();
        return s;
    }

    // An Id's name.
    Str ident(const Token &t)
    {
        Str lit = text(t);
        if (lit.size() > 1 && lit[1] != '"')
            return lit.substr(1);
        bytes.clear();
        if (!token_bytes(src, t, bytes))
            oom();
        Str s = arena.str(Str(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
        if (arena.failed())
            oom();
        return s;
    }

    // uN of `bits`.
    u64 nat(const Token &t, u32 bits)
    {
        u64 v = 0;
        if (t.kind != Tok::Nat)
            unexpected(t);
        else if (!parse_uint(text(t), bits, v))
            fail(t, bits == 8    ? "i8 constant out of range"_s
                    : bits == 32 ? "i32 constant out of range"_s
                                 : "i64 constant out of range"_s);
        return v;
    }

    bool is_num(u32 i = 0)
    {
        Tok k = peek(i).kind;
        return k == Tok::Nat || k == Tok::Int || k == Tok::Float;
    }

    bool is_idx(u32 i = 0)
    {
        Tok k = peek(i).kind;
        return k == Tok::Nat || k == Tok::Id;
    }

    Idx idx()
    {
        Token t = take();
        Idx x;
        x.loc = loc(t);
        if (t.kind == Tok::Id) {
            x.kind = Idx::Kind::Id;
            x.id   = ident(t);
        } else {
            x.value = u32(nat(t, 32));
        }
        return x;
    }

    // An index left out: 0, where it would have been.
    static Idx zero(Loc at)
    {
        Idx x;
        x.loc = at;
        return x;
    }

    // id? and (@name …)?
    Bind bind()
    {
        Bind b;
        if (is(Tok::Id)) {
            b.id.has   = true;
            b.id.value = ident(take());
        }
        annot_name(b);
        return b;
    }

    // ---------------------------------------------------- types

    HeapType heap_type()
    {
        HeapType h;
        i32 k = keyword_in(ABS, peek());
        if (k >= 0) {
            take();
            h.abs = AbsHeapType(k);
        } else if (is_idx()) {
            h.kind  = HeapType::Kind::Concrete;
            h.index = idx();
        } else {
            unexpected(peek());
        }
        return h;
    }

    bool is_ref_type(u32 i = 0)
    {
        return keyword_in(REFS, peek(i)) >= 0 || (is(Tok::LParen, i) && is_kw("ref", i + 1));
    }

    RefType ref_type()
    {
        RefType r;
        i32 k = keyword_in(REFS, peek());
        if (k >= 0) {
            take();
            r.nullable = true;
            r.heap.abs = AbsHeapType(k);
            return r;
        }
        if (!open_kw("ref")) {
            unexpected(peek(is(Tok::LParen) ? 1 : 0));
            return r;
        }
        enter();
        if (is_kw("null")) {
            take();
            r.nullable = true;
        }
        r.heap = heap_type();
        close();
        return r;
    }

    ValType val_type()
    {
        ValType v;
        i32 k = keyword_in(NUMS, peek());
        if (k >= 0) {
            take();
            v.kind = ValType::Kind(k);
        } else if (is_ref_type()) {
            v.kind = ValType::Kind::Ref;
            v.type = ref_type();
        } else {
            unexpected(peek(is(Tok::LParen) ? 1 : 0));
        }
        return v;
    }

    // (param …)* (result …)*. A param has an id only where `ids`.
    void params_results(Vec<Param> &ps, Vec<ValType> &rs, bool ids)
    {
        while (!failed && open_kw("param")) {
            enter();
            if (is(Tok::Id) && !ids) {
                unexpected(peek());
                return;
            }
            Param p;
            if (ids)
                p.bind = bind();
            if (p.bind.id.has || p.bind.name.has) {
                p.type = val_type();
                add(ps, p);
            } else {
                while (!failed && !is(Tok::RParen)) {
                    p.type = val_type();
                    add(ps, p);
                }
            }
            close();
        }
        while (!failed && open_kw("result")) {
            enter();
            while (!failed && !is(Tok::RParen))
                add(rs, val_type());
            close();
        }
    }

    TypeUse type_use(bool ids)
    {
        TypeUse u;
        if (open_kw("type")) {
            enter();
            u.type.has   = true;
            u.type.value = idx();
            close();
        }
        Vec<Param> ps;
        Vec<ValType> rs;
        params_results(ps, rs, ids);
        u.params  = list(ps);
        u.results = list(rs);
        return u;
    }

    BlockType block_type()
    {
        TypeUse u = type_use(false);
        BlockType b;
        if (u.type.has || !u.params.empty() || u.results.size() > 1) {
            b.kind = BlockType::Kind::Use;
            b.use  = u;
        } else if (u.results.size() == 1) {
            b.kind   = BlockType::Kind::Result;
            b.result = u.results[0];
        }
        return b;
    }

    StorageType storage()
    {
        StorageType s;
        if (is_kw("i8") || is_kw("i16")) {
            s.kind = is_kw("i8") ? StorageType::Kind::I8 : StorageType::Kind::I16;
            take();
        } else {
            s.type = val_type();
        }
        return s;
    }

    FieldType field_type()
    {
        FieldType f;
        if (open_kw("mut")) {
            enter();
            f.storage  = storage();
            f.mutable_ = true;
            close();
        } else {
            f.storage = storage();
        }
        return f;
    }

    CompType comp_type()
    {
        CompType c;
        if (open_kw("func")) {
            enter();
            Vec<Param> ps;
            Vec<ValType> rs;
            params_results(ps, rs, true);
            c.params  = list(ps);
            c.results = list(rs);
        } else if (open_kw("struct")) {
            enter();
            c.kind = CompType::Kind::StructType;
            Vec<Field> fs;
            while (!failed && open_kw("field")) {
                enter();
                Field f;
                f.bind = bind();
                if (f.bind.id.has || f.bind.name.has) {
                    f.type = field_type();
                    add(fs, f);
                } else {
                    while (!failed && !is(Tok::RParen)) {
                        f.type = field_type();
                        add(fs, f);
                    }
                }
                close();
            }
            c.fields = list(fs);
        } else if (open_kw("array")) {
            enter();
            c.kind  = CompType::Kind::ArrayType;
            c.field = field_type();
        } else {
            unexpected(peek(is(Tok::LParen) ? 1 : 0));
            return c;
        }
        close();
        return c;
    }

    SubType sub_type()
    {
        SubType s;
        if (!open_kw("sub")) {
            s.body = comp_type();
            return s;
        }
        enter();
        s.final = is_kw("final");
        if (s.final)
            take();
        Vec<Idx> supers;
        while (!failed && is_idx())
            add(supers, idx());
        s.supers = list(supers);
        s.body   = comp_type();
        close();
        return s;
    }

    // After (type: id? subtype ')'.
    TypeDef type_def()
    {
        TypeDef d;
        d.bind = bind();
        d.type = sub_type();
        close();
        return d;
    }

    AddrType addr_type()
    {
        Token t = peek();
        if (is_kw("i64")) {
            take();
            return AddrType::Addr64;
        }
        if (is_kw("i32"))
            take();
        else if (is_kw("f32") || is_kw("f64"))
            fail(t, "malformed address type");
        return AddrType::Addr32;
    }

    Limits limits()
    {
        Limits l;
        l.min = nat(take(), 64);
        if (is(Tok::Nat)) {
            l.max.has   = true;
            l.max.value = nat(take(), 64);
        }
        return l;
    }

    // (pagesize n)
    Opt<u64> page_size()
    {
        Opt<u64> p;
        if (!open_kw("pagesize"))
            return p;
        enter();
        p.has   = true;
        p.value = nat(take(), 64);
        close();
        return p;
    }

    MemType mem_type()
    {
        MemType m;
        m.addr   = addr_type();
        m.limits = limits();
        if (is_kw("shared")) {
            take();
            m.shared = true;
        }
        m.page_size = page_size();
        return m;
    }

    TableType table_type()
    {
        TableType t;
        t.addr   = addr_type();
        t.limits = limits();
        t.elem   = ref_type();
        return t;
    }

    GlobalType global_type()
    {
        GlobalType g;
        if (open_kw("mut")) {
            enter();
            g.type     = val_type();
            g.mutable_ = true;
            close();
        } else {
            g.type = val_type();
        }
        return g;
    }

    // ---------------------------------------------------- immediates

    // offset=? align=?, the alignment natural when left out.
    void mem_arg(const OpDef &d, u64 &offset, u64 &align)
    {
        offset  = 0;
        align   = u64(1) << d.align;
        Token t = peek();
        if (t.kind == Tok::Keyword && text(t).starts_with("offset=")) {
            take();
            if (!parse_uint(text(t).substr(7), 64, offset))
                fail(t, "i64 constant out of range");
            t = peek();
        }
        if (t.kind == Tok::Keyword && text(t).starts_with("align=")) {
            take();
            u64 a = 0;
            if (!parse_uint(text(t).substr(6), 64, a))
                fail(t, "i64 constant out of range");
            else if (!a || (a & (a - 1)))
                fail(t, "alignment must be a power of two");
            align = a;
        }
    }

    bool is_mem_kw(u32 i)
    {
        Token t = peek(i);
        return t.kind == Tok::Keyword &&
               (text(t).starts_with("offset=") || text(t).starts_with("align="));
    }

    // A list of immediates is over: what follows can follow an instruction.
    // The reference looks at it before it counts the list.
    void ended()
    {
        Token t = peek();
        if (t.kind != Tok::LParen && t.kind != Tok::RParen && t.kind != Tok::Keyword &&
            t.kind != Tok::Eof)
            unexpected(t);
    }

    template <class T>
    T *make(u16 op, const Token &kw)
    {
        T *n = node<T>(loc(kw));
        if constexpr (requires { n->op; })
            n->op = op;
        return n;
    }

    // v128.const shape lane*
    Instr *v128_const(const Token &kw)
    {
        auto *n = node<Instr::V128Const>(loc(kw));
        Token s = take();
        i32 k   = keyword_in(SHAPES, s);
        if (k < 0) {
            unexpected(s);
            return n;
        }
        Vec<Token> lanes;
        while (!failed && is_num())
            add(lanes, take());
        ended();
        if (!failed && lanes.size() != LANES[k]) {
            fail(kw, "wrong number of lane literals");
            return n;
        }
        char b[16];
        u32 size = 1u << LANE_LOG[k];
        for (u32 i = 0; i < lanes.size(); i++) {
            Str t = text(lanes[i]);
            u64 v = 0;
            u32 v32;
            bool ok = false;
            if (k == 4 && (ok = parse_f32(t, v32)))
                v = v32;
            else if (k == 5)
                ok = parse_f64(t, v);
            else if (k < 4)
                ok = parse_int(t, size * 8, v);
            if (!ok) {
                fail(lanes[i], "constant out of range");
                return n;
            }
            for (u32 j = 0; j < size; j++)
                b[i * size + j] = char(v >> (8 * j));
        }
        n->value = arena.str(Str(b, 16));
        if (arena.failed())
            oom();
        return n;
    }

    // The instruction `op`, its keyword taken, with its immediates: any
    // but a block's.
    Instr *immediates(u16 op, const Token &kw, bool has_hint, const Token &hint_at)
    {
        const OpDef &d = op_def(op);
        Loc at         = loc(kw);
        switch (d.imm) {
        case Imm::NONE:
            return make<Instr::Plain>(op, kw);
        case Imm::SELECT: {
            auto *n = make<Instr::Select>(op, kw);
            Vec<ValType> rs;
            while (!failed && open_kw("result")) {
                enter();
                n->typed = true;
                while (!failed && !is(Tok::RParen))
                    add(rs, val_type());
                close();
            }
            n->results = list(rs);
            return n;
        }
        case Imm::LABEL: {
            auto *n  = make<Instr::Br>(op, kw);
            n->label = idx();
            if (d.name == "br_if")
                n->hint = branch_hint(has_hint, hint_at);
            return n;
        }
        case Imm::BR_TABLE: {
            auto *n = make<Instr::BrTable>(op, kw);
            Vec<Idx> ls;
            add(ls, idx());
            while (!failed && is_idx())
                add(ls, idx());
            if (failed)
                return n;
            n->default_ = ls.back();
            ls.pop();
            n->labels = list(ls);
            return n;
        }
        case Imm::BR_ON_CAST: {
            auto *n  = make<Instr::BrOnCast>(op, kw);
            n->label = idx();
            n->from  = ref_type();
            n->to    = ref_type();
            return n;
        }
        case Imm::IDX:
        case Imm::IDX_OPT: {
            auto *n = make<Instr::Index>(op, kw);
            n->x    = d.imm == Imm::IDX || is_idx() ? idx() : zero(at);
            return n;
        }
        case Imm::IDX2:
        case Imm::IDX2_OPT: {
            auto *n = make<Instr::Index2>(op, kw);
            if (d.imm == Imm::IDX2 || is_idx()) {
                n->x = idx();
                n->y = idx();
            } else {
                n->x = n->y = zero(at);
            }
            return n;
        }
        case Imm::IDX_OPT_IDX: {
            auto *n = make<Instr::Index2>(op, kw);
            Idx a   = idx();
            if (is_idx()) {
                n->x = a;
                n->y = idx();
            } else {
                n->x = zero(at);
                n->y = a;
            }
            return n;
        }
        case Imm::NEW_FIXED: {
            auto *n  = make<Instr::ArrayNewFixed>(op, kw);
            n->type  = idx();
            n->count = u32(nat(take(), 32));
            return n;
        }
        case Imm::CALL_INDIRECT: {
            auto *n  = make<Instr::CallIndirect>(op, kw);
            n->table = is_idx() ? idx() : zero(at);
            n->type  = type_use(false);
            return n;
        }
        case Imm::MEM: {
            auto *n   = make<Instr::MemArg>(op, kw);
            n->memory = is_idx() ? idx() : zero(at);
            mem_arg(d, n->offset, n->align);
            return n;
        }
        case Imm::MEM_LANE: {
            // The last number is the lane: a memory index is an id, or a
            // number that another number or a memory operand follows.
            auto *n   = make<Instr::MemArgLane>(op, kw);
            bool x    = is(Tok::Id) || (is(Tok::Nat) && (is(Tok::Nat, 1) || is_mem_kw(1)));
            n->memory = x ? idx() : zero(at);
            mem_arg(d, n->offset, n->align);
            n->lane = u8(nat(take(), 8));
            return n;
        }
        case Imm::I32:
        case Imm::I64:
        case Imm::F32:
        case Imm::F64:
            return constant(op, kw);
        case Imm::V128:
            return v128_const(kw);
        case Imm::LANE: {
            auto *n = make<Instr::Lane>(op, kw);
            n->lane = u8(nat(take(), 8));
            return n;
        }
        case Imm::SHUFFLE: {
            auto *n = make<Instr::Shuffle>(op, kw);
            Vec<u8> lanes;
            while (!failed && is(Tok::Nat))
                add(lanes, u8(nat(take(), 8)));
            ended();
            if (!failed && lanes.size() != 16)
                fail(kw, "wrong number of lane indices");
            n->lanes = list(lanes);
            return n;
        }
        case Imm::HEAP_TYPE: {
            auto *n = make<Instr::RefNull>(op, kw);
            n->type = heap_type();
            return n;
        }
        case Imm::REF_TYPE: {
            auto *n = make<Instr::RefTypeOp>(op, kw);
            n->type = ref_type();
            return n;
        }
        default:
            unexpected(kw);
            return make<Instr::Plain>(op, kw);
        }
    }

    Instr *constant(u16 op, const Token &kw)
    {
        Token t = take();
        if (t.kind != Tok::Nat && t.kind != Tok::Int && t.kind != Tok::Float) {
            unexpected(t);
            return make<Instr::Plain>(op, kw);
        }
        Str s   = text(t);
        u64 v   = 0;
        bool ok = false;
        Instr *n;
        switch (op_def(op).imm) {
        case Imm::I32: {
            auto *c  = make<Instr::I32Const>(op, kw);
            ok       = parse_int(s, 32, v);
            c->value = u32(v);
            n        = c;
            break;
        }
        case Imm::I64: {
            auto *c  = make<Instr::I64Const>(op, kw);
            ok       = parse_int(s, 64, v);
            c->value = v;
            n        = c;
            break;
        }
        case Imm::F32: {
            auto *c = make<Instr::F32Const>(op, kw);
            ok      = parse_f32(s, c->bits);
            n       = c;
            break;
        }
        default: {
            auto *c = make<Instr::F64Const>(op, kw);
            ok      = parse_f64(s, c->bits);
            n       = c;
            break;
        }
        }
        if (!ok)
            fail(t, "constant out of range");
        return n;
    }

    // ---------------------------------------------------- instructions

    Frame &top() { return stack.back(); }

    void push(Frame &&f)
    {
        if (!stack.push(move(f)))
            oom();
    }

    // Flat instructions may stand here.
    static bool body(const Frame &f)
    {
        switch (f.kind) {
        case F::Root:
        case F::Flat:
            return true;
        case F::Plain:
            return false;
        case F::Folded:
            if (f.imm == Imm::IF)
                return f.phase == THEN || f.phase == ELSE;
            if (f.imm == Imm::TRY)
                return f.phase == DO || f.phase == CATCH;
            return true;
        }
        return false;
    }

    // Folded instructions may stand here.
    static bool operands(const Frame &f)
    {
        return body(f) || f.kind == F::Plain ||
               (f.kind == F::Folded && f.imm == Imm::IF && f.phase == COND);
    }

    // A ')' closes this frame, or a part of it.
    static bool closes(const Frame &f)
    {
        switch (f.kind) {
        case F::Root:
        case F::Flat:
            return false;
        case F::Plain:
            return true;
        case F::Folded:
            if (f.imm == Imm::IF)
                return f.phase != COND;
            if (f.imm == Imm::TRY)
                return f.phase != DO_NEXT;
            return true;
        }
        return false;
    }

    // A block's head, its keyword taken: label, type, and try_table's catches.
    void block_head(Frame &f, u16 op, const Token &kw)
    {
        f.imm   = op_def(op).imm;
        f.op    = op;
        f.at    = kw;
        f.label = bind();
        f.type  = block_type();
        if (f.imm != Imm::TRY_TABLE)
            return;
        Vec<Catch> cs;
        for (;;) {
            Catch c;
            if (open_kw("catch"))
                c.kind = Catch::Kind::Catch;
            else if (open_kw("catch_ref"))
                c.kind = Catch::Kind::CatchRef;
            else if (open_kw("catch_all"))
                c.kind = Catch::Kind::CatchAll;
            else if (open_kw("catch_all_ref"))
                c.kind = Catch::Kind::CatchAllRef;
            else
                break;
            enter();
            if (c.kind == Catch::Kind::Catch || c.kind == Catch::Kind::CatchRef)
                c.tag = idx();
            c.label = idx();
            close();
            if (failed)
                break;
            add(cs, c);
        }
        f.catches = list(cs);
    }

    // The node a finished frame stands for.
    Instr *build(Frame &f)
    {
        Loc at = loc(f.at);
        switch (f.imm) {
        case Imm::BLOCK: {
            auto *n  = node<Instr::Block>(at);
            n->op    = f.op;
            n->label = f.label;
            n->type  = f.type;
            n->body  = list(f.seq);
            return n;
        }
        case Imm::IF: {
            if (f.phase == THEN)
                f.first = list(f.seq);
            else if (f.phase == ELSE)
                f.else_body = list(f.seq);
            auto *n      = node<Instr::If>(at);
            n->label     = f.label;
            n->type      = f.type;
            n->then_body = f.first;
            n->has_else  = f.has_else;
            n->else_body = f.else_body;
            n->hint      = f.hint;
            return n;
        }
        case Imm::TRY_TABLE: {
            auto *n    = node<Instr::TryTable>(at);
            n->label   = f.label;
            n->type    = f.type;
            n->catches = f.catches;
            n->body    = list(f.seq);
            return n;
        }
        case Imm::TRY: {
            if (f.phase == DO) {
                f.first = list(f.seq);
            } else if (f.phase == CATCH) {
                f.cur.body = list(f.seq);
                add(f.legacy, f.cur);
            }
            if (f.delegated) {
                auto *n      = node<Instr::TryDelegate>(at);
                n->label     = f.label;
                n->type      = f.type;
                n->body      = f.first;
                n->label_out = f.label_out;
                return n;
            }
            auto *n    = node<Instr::Try>(at);
            n->label   = f.label;
            n->type    = f.type;
            n->body    = f.first;
            n->catches = list(f.legacy);
            return n;
        }
        default:
            return f.plain;
        }
    }

    // The top frame is done: its instructions go to the one below.
    void finish()
    {
        Frame f = move(top());
        stack.pop();
        Instr *in = build(f);
        Frame &p  = top();
        for (Instr *x : f.kind == F::Plain ? f.seq : f.cond)
            add(p.seq, x);
        add(p.seq, in);
        if (stack.size() == 1)
            one_done = true;
    }

    // `end $l` or `else $l`: the label again, if any.
    void end_label(const Frame &f)
    {
        if (!is(Tok::Id))
            return;
        Token t = take();
        if (!f.label.id.has || f.label.id.value != ident(t))
            fail(t, "mismatching label");
    }

    // A ')' the top frame takes.
    void rparen()
    {
        take();
        Frame &f = top();
        if (f.kind == F::Folded && f.imm == Imm::IF && (f.phase == THEN || f.phase == ELSE)) {
            (f.phase == THEN ? f.first : f.else_body) = list(f.seq);
            f.seq.clear();
            f.phase = f.phase == THEN ? AFTER_THEN : AFTER_ELSE;
        } else if (f.kind == F::Folded && f.imm == Imm::TRY && f.phase == DO) {
            f.first = list(f.seq);
            f.seq.clear();
            f.phase = AFTER_DO;
        } else if (f.kind == F::Folded && f.imm == Imm::TRY && f.phase == CATCH) {
            f.cur.body = list(f.seq);
            add(f.legacy, f.cur);
            f.seq.clear();
            f.phase = AFTER_DO;
        } else {
            finish();
        }
    }

    // A '(' in the instructions: a part of a folded if or try, or a folded
    // instruction.
    void lparen()
    {
        Frame &f = top();
        Token k  = peek(1);
        Str w    = k.kind == Tok::Keyword ? text(k) : Str();
        if (f.kind == F::Folded && f.imm == Imm::IF) {
            if (f.phase == COND && w == "then") {
                enter();
                f.cond = move(f.seq);
                f.seq.clear();
                f.phase = THEN;
                return;
            }
            if (f.phase == AFTER_THEN && w == "else") {
                enter();
                f.has_else = true;
                f.phase    = ELSE;
                return;
            }
        }
        if (f.kind == F::Folded && f.imm == Imm::TRY) {
            if (f.phase == DO_NEXT && w == "do") {
                enter();
                f.phase = DO;
                return;
            }
            if (f.phase == AFTER_DO && !f.all && (w == "catch" || w == "catch_all")) {
                enter();
                f.cur = LegacyCatch();
                if (w == "catch")
                    f.cur.tag = idx();
                else
                    f.cur.kind = LegacyCatch::Kind::LegacyCatchAll;
                f.all   = w == "catch_all";
                f.phase = CATCH;
                return;
            }
            if (f.phase == AFTER_DO && !f.all && f.legacy.empty() && w == "delegate") {
                enter();
                f.label_out = idx();
                close();
                f.delegated = true;
                f.phase     = DONE;
                return;
            }
        }
        u16 op;
        if (!operands(f) || !find_op(w, op)) {
            unexpected(k);
            return;
        }
        take();
        bool has_hint  = hinted;
        Token hint_at  = hint;
        Token kw       = take();
        const OpDef &d = op_def(op);
        Frame n;
        n.at = kw;
        if (d.imm == Imm::BLOCK || d.imm == Imm::IF || d.imm == Imm::TRY_TABLE ||
            d.imm == Imm::TRY) {
            n.kind = F::Folded;
            block_head(n, op, kw);
            n.phase = d.imm == Imm::IF ? COND : d.imm == Imm::TRY ? DO_NEXT : 0;
            if (d.imm == Imm::IF)
                n.hint = branch_hint(has_hint, hint_at);
        } else {
            n.kind  = F::Plain;
            n.plain = immediates(op, kw, has_hint, hint_at);
        }
        push(move(n));
    }

    // A keyword in the instructions: false where it ends them.
    bool keyword()
    {
        Frame &f      = top();
        Token t       = peek();
        Str w         = text(t);
        bool flat_if  = f.kind == F::Flat && f.imm == Imm::IF;
        bool flat_try = f.kind == F::Flat && f.imm == Imm::TRY;
        if (w == "end" && f.kind == F::Flat) {
            take();
            end_label(f);
            finish();
            return true;
        }
        if (w == "else" && flat_if && f.phase == THEN) {
            take();
            end_label(f);
            f.first = list(f.seq);
            f.seq.clear();
            f.has_else = true;
            f.phase    = ELSE;
            return true;
        }
        if ((w == "catch" || w == "catch_all") && flat_try && !f.all) {
            take();
            if (f.phase == DO) {
                f.first = list(f.seq);
            } else {
                f.cur.body = list(f.seq);
                add(f.legacy, f.cur);
            }
            f.seq.clear();
            f.cur = LegacyCatch();
            if (w == "catch")
                f.cur.tag = idx();
            else
                f.cur.kind = LegacyCatch::Kind::LegacyCatchAll;
            f.all   = w == "catch_all";
            f.phase = CATCH;
            return true;
        }
        if (w == "delegate" && flat_try && f.phase == DO) {
            take();
            f.label_out = idx();
            f.delegated = true;
            finish();
            return true;
        }
        u16 op;
        if (!find_op(w, op) || !body(f)) {
            if (f.kind == F::Root)
                return false;
            unexpected(t);
            return true;
        }
        take();
        const OpDef &d = op_def(op);
        if (d.imm == Imm::BLOCK || d.imm == Imm::IF || d.imm == Imm::TRY_TABLE ||
            d.imm == Imm::TRY) {
            bool has_hint = hinted;
            Token hint_at = hint;
            Frame n;
            n.kind = F::Flat;
            block_head(n, op, t);
            n.phase = d.imm == Imm::IF ? THEN : d.imm == Imm::TRY ? DO : 0;
            if (d.imm == Imm::IF)
                n.hint = branch_hint(has_hint, hint_at);
            push(move(n));
            return true;
        }
        bool has_hint = hinted;
        Token hint_at = hint;
        Instr *in     = immediates(op, t, has_hint, hint_at);
        add(top().seq, in);
        return true;
    }

    // Instructions into `out`, up to what cannot continue them: the ')'
    // closing the field, which is not taken. With `one`, exactly one
    // folded instruction.
    void instrs(Vec<Instr *> &out, bool one)
    {
        stack.clear();
        push(Frame());
        one_done = false;
        if (one && !(is(Tok::LParen) && peek(1).kind == Tok::Keyword))
            unexpected(peek(is(Tok::LParen) ? 1 : 0));
        while (!failed && !(one && one_done)) {
            Frame &f = top();
            Token t  = peek();
            if (t.kind == Tok::RParen) {
                if (f.kind == F::Root)
                    break;
                if (!closes(f)) {
                    unexpected(t);
                    break;
                }
                rparen();
            } else if (t.kind == Tok::LParen) {
                lparen();
            } else if (t.kind == Tok::Keyword && !(one && f.kind == F::Root)) {
                if (!keyword())
                    break;
            } else {
                if (f.kind != F::Root)
                    unexpected(t);
                break;
            }
        }
        if (!failed)
            for (Instr *x : stack[0].seq)
                add(out, x);
        stack.clear();
    }

    List<Instr *> expr_list(bool one)
    {
        Vec<Instr *> v;
        instrs(v, one);
        return list(v);
    }

    // (offset instr*), or one folded instruction.
    List<Instr *> offset()
    {
        if (!open_kw("offset"))
            return expr_list(true);
        enter();
        List<Instr *> l = expr_list(false);
        close();
        return l;
    }

    // (item instr*), or one folded instruction.
    Expr elem_expr()
    {
        Expr x;
        if (!open_kw("item")) {
            x.instrs = expr_list(true);
            return x;
        }
        enter();
        x.instrs = expr_list(false);
        close();
        return x;
    }

    bool is_folded_instr()
    {
        u16 op;
        return is(Tok::LParen) && peek(1).kind == Tok::Keyword && find_op(text(peek(1)), op);
    }

    // ---------------------------------------------------- module fields

    void note(Kind k) { add(seen, Seen{ k, field_at }); }

    void decl(Decl *d) { add(decls, d); }

    // (export "name")*
    List<Str> exports()
    {
        Vec<Str> v;
        while (!failed && open_kw("export")) {
            enter();
            add(v, name(take()));
            close();
        }
        return list(v);
    }

    // (import "module" "item"), into `d`.
    bool inline_import(Decl::Import *&d, const Token &kw)
    {
        if (!open_kw("import"))
            return false;
        enter();
        d         = node<Decl::Import>(loc(kw));
        d->module = name(take());
        d->item   = name(take());
        close();
        note(K_IMPORT);
        return true;
    }

    void type_field(const Token &kw)
    {
        auto *d = node<Decl::Type>(loc(kw));
        Vec<TypeDef> group;
        if (text(kw) == "type") {
            add(group, type_def());
        } else {
            while (!failed && open_kw("type")) {
                enter();
                add(group, type_def());
            }
            close();
        }
        d->group = list(group);
        decl(d);
    }

    void import_field(const Token &kw)
    {
        auto *d   = node<Decl::Import>(loc(kw));
        d->module = name(take());
        d->item   = name(take());
        Token lp  = take();
        Token k   = take();
        Extern &e = d->desc;
        Str w     = text(k);
        if (lp.kind != Tok::LParen || k.kind != Tok::Keyword) {
            unexpected(lp.kind != Tok::LParen ? lp : k);
            return;
        }
        if (w == "func" || w == "tag") {
            e.kind = w == "func" ? Extern::Kind::FuncImport : Extern::Kind::TagImport;
            e.bind = bind();
            e.type = type_use(true);
        } else if (w == "table") {
            e.kind  = Extern::Kind::TableImport;
            e.bind  = bind();
            e.table = table_type();
        } else if (w == "memory") {
            e.kind   = Extern::Kind::MemoryImport;
            e.bind   = bind();
            e.memory = mem_type();
        } else if (w == "global") {
            e.kind   = Extern::Kind::GlobalImport;
            e.bind   = bind();
            e.global = global_type();
        } else {
            unexpected(k);
            return;
        }
        close();
        close();
        note(K_IMPORT);
        decl(d);
    }

    void func_field(const Token &kw)
    {
        Bind b       = bind();
        List<Str> ex = exports();
        Decl::Import *im;
        if (inline_import(im, kw)) {
            im->desc.bind = b;
            im->desc.type = type_use(true);
            im->exports   = ex;
            close();
            decl(im);
            return;
        }
        auto *f    = node<Decl::Func>(loc(kw));
        f->bind    = b;
        f->exports = ex;
        f->type    = type_use(true);
        Vec<Local> ls;
        while (!failed && open_kw("local")) {
            enter();
            Local l;
            l.bind = bind();
            if (l.bind.id.has || l.bind.name.has) {
                l.type = val_type();
                add(ls, l);
            } else {
                while (!failed && !is(Tok::RParen)) {
                    l.type = val_type();
                    add(ls, l);
                }
            }
            close();
        }
        f->locals = list(ls);
        f->body   = expr_list(false);
        close();
        note(K_FUNC);
        decl(f);
    }

    // ref.null of a table's heap type: its init when it has none.
    List<Instr *> ref_null(const RefType &rt, const Token &kw)
    {
        auto *n = node<Instr::RefNull>(loc(kw));
        n->type = rt.heap;
        Vec<Instr *> v;
        add(v, static_cast<Instr *>(n));
        return list(v);
    }

    void table_field(const Token &kw)
    {
        Bind b       = bind();
        List<Str> ex = exports();
        Decl::Import *im;
        if (inline_import(im, kw)) {
            im->desc.kind  = Extern::Kind::TableImport;
            im->desc.bind  = b;
            im->desc.table = table_type();
            im->exports    = ex;
            close();
            decl(im);
            return;
        }
        auto *t      = node<Decl::Table>(loc(kw));
        t->bind      = b;
        t->exports   = ex;
        t->type.addr = addr_type();
        if (is(Tok::Nat)) {
            t->type.limits = limits();
            t->type.elem   = ref_type();
            t->init        = expr_list(false);
            if (t->init.empty())
                t->init = ref_null(t->type.elem, kw);
        } else {
            // (table at? reftype (elem …)): its limits are the count.
            t->type.elem = ref_type();
            if (!open_kw("elem")) {
                unexpected(peek(is(Tok::LParen) ? 1 : 0));
                return;
            }
            enter();
            ElemList &l  = t->elems.value;
            t->elems.has = true;
            u64 n        = 0;
            if (is(Tok::LParen)) {
                l.kind = ElemList::Kind::Exprs;
                l.type = t->type.elem;
                Vec<Expr> items;
                while (!failed && is(Tok::LParen))
                    add(items, elem_expr());
                l.items = list(items);
                n       = l.items.size();
            } else {
                Vec<Idx> fs;
                while (!failed && is_idx())
                    add(fs, idx());
                l.funcs = list(fs);
                n       = l.funcs.size();
            }
            close();
            t->type.limits.min = n;
            t->type.limits.max = { true, n };
            t->init            = ref_null(t->type.elem, kw);
        }
        close();
        note(K_TABLE);
        decl(t);
    }

    void memory_field(const Token &kw)
    {
        Bind b       = bind();
        List<Str> ex = exports();
        Decl::Import *im;
        if (inline_import(im, kw)) {
            im->desc.kind   = Extern::Kind::MemoryImport;
            im->desc.bind   = b;
            im->desc.memory = mem_type();
            im->exports     = ex;
            close();
            decl(im);
            return;
        }
        auto *m    = node<Decl::Memory>(loc(kw));
        m->bind    = b;
        m->exports = ex;
        if (open_kw("data") || open_kw("pagesize") ||
            ((is_kw("i32") || is_kw("i64")) && is(Tok::LParen, 1))) {
            // (memory at? pagesize? (data …)): its limits are the size.
            m->type.addr      = addr_type();
            m->type.page_size = page_size();
            if (!open_kw("data")) {
                unexpected(peek(is(Tok::LParen) ? 1 : 0));
                return;
            }
            enter();
            m->data.has   = true;
            m->data.value = strings();
            close();
            u64 page           = m->type.page_size.has ? m->type.page_size.value : 65536;
            u64 n              = page ? (m->data.value.size() + page - 1) / page : 0;
            m->type.limits.min = n;
            m->type.limits.max = { true, n };
        } else {
            m->type = mem_type();
        }
        close();
        note(K_MEMORY);
        decl(m);
    }

    void global_field(const Token &kw)
    {
        Bind b       = bind();
        List<Str> ex = exports();
        Decl::Import *im;
        if (inline_import(im, kw)) {
            im->desc.kind   = Extern::Kind::GlobalImport;
            im->desc.bind   = b;
            im->desc.global = global_type();
            im->exports     = ex;
            close();
            decl(im);
            return;
        }
        auto *g    = node<Decl::Global>(loc(kw));
        g->bind    = b;
        g->exports = ex;
        g->type    = global_type();
        g->init    = expr_list(false);
        close();
        note(K_GLOBAL);
        decl(g);
    }

    void tag_field(const Token &kw)
    {
        Bind b       = bind();
        List<Str> ex = exports();
        Decl::Import *im;
        if (inline_import(im, kw)) {
            im->desc.kind = Extern::Kind::TagImport;
            im->desc.bind = b;
            im->desc.type = type_use(true);
            im->exports   = ex;
            close();
            decl(im);
            return;
        }
        auto *t    = node<Decl::Tag>(loc(kw));
        t->bind    = b;
        t->exports = ex;
        t->type    = type_use(true);
        close();
        note(K_TAG);
        decl(t);
    }

    void export_field(const Token &kw)
    {
        auto *e  = node<Decl::Export>(loc(kw));
        e->name  = name(take());
        Token lp = take();
        Token k  = take();
        Str w    = text(k);
        if (lp.kind != Tok::LParen || k.kind != Tok::Keyword) {
            unexpected(lp.kind != Tok::LParen ? lp : k);
            return;
        }
        if (w == "func")
            e->sort = ExternKind::FuncKind;
        else if (w == "table")
            e->sort = ExternKind::TableKind;
        else if (w == "memory")
            e->sort = ExternKind::MemoryKind;
        else if (w == "global")
            e->sort = ExternKind::GlobalKind;
        else if (w == "tag")
            e->sort = ExternKind::TagKind;
        else {
            unexpected(k);
            return;
        }
        e->index = idx();
        close();
        close();
        decl(e);
    }

    void start_field(const Token &kw)
    {
        auto *s = node<Decl::Start>(loc(kw));
        s->func = idx();
        close();
        note(K_START);
        decl(s);
    }

    // func x* | reftype item*; x* alone where `bare`.
    ElemList elem_list(bool bare)
    {
        ElemList l;
        if (is_kw("func") || (bare && (is_idx() || is(Tok::RParen)))) {
            if (is_kw("func"))
                take();
            Vec<Idx> fs;
            while (!failed && is_idx())
                add(fs, idx());
            l.funcs = list(fs);
        } else if (is_ref_type()) {
            l.kind = ElemList::Kind::Exprs;
            l.type = ref_type();
            Vec<Expr> items;
            while (!failed && is(Tok::LParen))
                add(items, elem_expr());
            l.items = list(items);
        } else {
            unexpected(peek(is(Tok::LParen) ? 1 : 0));
        }
        return l;
    }

    void elem_field(const Token &kw)
    {
        auto *e = node<Decl::Elem>(loc(kw));
        e->bind = bind();
        if (is_kw("declare")) {
            take();
            e->mode.kind = ElemMode::Kind::ElemDeclare;
            e->elems     = elem_list(false);
        } else if (open_kw("table")) {
            enter();
            e->mode.kind  = ElemMode::Kind::ElemActive;
            e->mode.table = idx();
            close();
            e->mode.offset = offset();
            e->elems       = elem_list(false);
        } else if (open_kw("offset") || is_folded_instr()) {
            e->mode.kind   = ElemMode::Kind::ElemActive;
            e->mode.table  = zero(loc(kw));
            e->mode.offset = offset();
            e->elems       = elem_list(true);
        } else {
            e->elems = elem_list(false);
        }
        close();
        decl(e);
    }

    void data_field(const Token &kw)
    {
        auto *d = node<Decl::Data>(loc(kw));
        d->bind = bind();
        if (open_kw("memory")) {
            enter();
            d->mode.kind   = DataMode::Kind::DataActive;
            d->mode.memory = idx();
            close();
            d->mode.offset = offset();
        } else if (open_kw("offset") || is_folded_instr()) {
            d->mode.kind   = DataMode::Kind::DataActive;
            d->mode.memory = zero(loc(kw));
            d->mode.offset = offset();
        }
        d->init = strings();
        close();
        decl(d);
    }

    void field()
    {
        Token k                          = peek(1);
        Str w                            = k.kind == Tok::Keyword ? text(k) : Str();
        void (Parser::*f)(const Token &) = nullptr;
        if (w == "type" || w == "rec")
            f = &Parser::type_field;
        else if (w == "import")
            f = &Parser::import_field;
        else if (w == "func")
            f = &Parser::func_field;
        else if (w == "table")
            f = &Parser::table_field;
        else if (w == "memory")
            f = &Parser::memory_field;
        else if (w == "global")
            f = &Parser::global_field;
        else if (w == "tag")
            f = &Parser::tag_field;
        else if (w == "export")
            f = &Parser::export_field;
        else if (w == "start")
            f = &Parser::start_field;
        else if (w == "elem")
            f = &Parser::elem_field;
        else if (w == "data")
            f = &Parser::data_field;
        if (!f) {
            unexpected(k);
            return;
        }
        field_at = peek();
        enter();
        (this->*f)(k);
    }

    void fields()
    {
        while (!failed) {
            customs();
            if (!is(Tok::LParen))
                break;
            field();
        }
    }

    // No import after a definition, and one start at most (§8), worded as
    // the reference words them.
    void rules()
    {
        u32 last = 0, imports = 0, starts = 0;
        for (u32 i = 0; i < seen.size(); i++)
            if (seen[i].kind == K_IMPORT) {
                last = i;
                imports++;
            }
        for (u32 i = imports ? last : 0; imports && i-- > 0;)
            if (seen[i].kind != K_IMPORT && seen[i].kind != K_START) {
                u32 k = i + 1;
                while (seen[k].kind != K_IMPORT)
                    k++;
                Out m;
                m.put("import after ").put(KIND_NAME[seen[i].kind]).put(" definition");
                fail(seen[k].at, m.str());
                return;
            }
        for (const Seen &s : seen)
            if (s.kind == K_START && ++starts == 2) {
                fail(s.at, "multiple start sections");
                return;
            }
    }

    void module(Module &m)
    {
        customs();
        if (is(Tok::LParen) && is_kw("module", 1)) {
            enter();
            m.bind = bind();
            fields();
            close();
        } else {
            fields();
        }
        customs();
        Token t = peek();
        if (!failed && t.kind != Tok::Eof)
            unexpected(t);
        if (!failed)
            rules();
        m.decls = list(decls);
    }
};

} // namespace

bool parse(Str name, Str source, Arena &arena, Module &m, Diag &diag)
{
    Parser p(name, source, arena, diag);
    p.module(m);
    return !p.failed;
}
