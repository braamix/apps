#include "parser.h"

#include "emit.h"
#include "lexer.h"
#include "number.h"
#include "optable.h"
#include "wasm.h"

namespace as {

namespace {

// Where a function is, as WebAssemblyAsmParser's state machine has it.
enum State : u8 {
    FILE_START,
    FUNCTION_LABEL,
    FUNCTION_START,
    FUNCTION_LOCALS,
    INSTRUCTIONS,
    END_FUNCTION,
    DATA_SECTION,
};

enum Nest : u8 { N_FUNCTION, N_BLOCK, N_LOOP, N_IF, N_ELSE };

// The operands an instruction takes, and so how it is encoded.
enum Form : u8 {
    NONE,          // the opcode alone
    BLOCK,         // a block type
    LABEL,         // a depth
    BR_TABLE,      // {depth, ...}
    LOCAL,         // a local's index
    GLOBAL,        // a global's symbol
    CALL,          // a function's symbol
    CALL_INDIRECT, // [table,] signature
    MEM,           // offset[:p2align=k]
    I32,
    I64,
    F32,
    F64,
    SELECT,  // value types, none for the plain select
    MEMIDX,  // a memory index
    MEMIDX2, // two
};

struct Instr {
    Form form;
    u8 prefix; // 0 for a one-byte opcode
    u16 sub;
    u8 align; // natural, log2
};

// An instruction by its llvm name; false for one outside the subset.
bool find_instr(Str name, Instr &in)
{
    struct Fixed {
        Str name;
        Form form;
        u8 op;
    };
    static constexpr Fixed STRUCTURAL[] = {
        { "block", BLOCK, 0x02 }, { "loop", BLOCK, 0x03 },        { "if", BLOCK, 0x04 },
        { "else", NONE, 0x05 },   { "end_block", NONE, 0x0b },    { "end_loop", NONE, 0x0b },
        { "end_if", NONE, 0x0b }, { "end_function", NONE, 0x0b },
    };
    for (const Fixed &f : STRUCTURAL)
        if (f.name == name) {
            in = Instr{ f.form, 0, f.op, 0 };
            return true;
        }
    u16 op;
    if (!find_op(name, op))
        return false;
    const OpDef &d = op_def(op);
    in             = Instr{ NONE, d.prefix, d.sub, d.align };
    switch (d.imm) {
    case Imm::NONE:
        // Control, the MVP's numbers, sign extension and trunc_sat.
        if (d.prefix == 0)
            return d.sub <= 0x01 || d.sub == 0x0f || d.sub == 0x1a ||
                   (d.sub >= 0x45 && d.sub <= 0xc4);
        return d.prefix == 0xfc && d.sub <= 7;
    case Imm::LABEL:
        in.form = LABEL;
        return name == "br" || name == "br_if";
    case Imm::BR_TABLE:
        in.form = BR_TABLE;
        return true;
    case Imm::IDX:
        if (name == "call")
            in.form = CALL;
        else if (d.x == Space::LOCAL)
            in.form = LOCAL;
        else if (d.x == Space::GLOBAL)
            in.form = GLOBAL;
        else
            return false;
        return true;
    case Imm::CALL_INDIRECT:
        in.form = CALL_INDIRECT;
        return name == "call_indirect";
    case Imm::MEM:
        in.form = MEM;
        return d.prefix == 0;
    case Imm::I32:
        in.form = I32;
        return true;
    case Imm::I64:
        in.form = I64;
        return true;
    case Imm::F32:
        in.form = F32;
        return true;
    case Imm::F64:
        in.form = F64;
        return true;
    case Imm::SELECT:
        in.form = SELECT;
        return name == "select";
    case Imm::IDX_OPT:
        in.form = MEMIDX;
        return name == "memory.size" || name == "memory.grow" || name == "memory.fill";
    case Imm::IDX2_OPT:
        in.form = MEMIDX2;
        return name == "memory.copy";
    default:
        return false;
    }
}

// A value type by its name, as WebAssembly::parseType reads it.
bool value_type(Str s, u8 &t)
{
    struct Name {
        Str name;
        u8 type;
    };
    static constexpr Name NAMES[] = {
        { "i32", 0x7f },       { "i64", 0x7e },    { "f32", 0x7d },   { "f64", 0x7c },
        { "v128", 0x7b },      { "i8x16", 0x7b },  { "i16x8", 0x7b }, { "i32x4", 0x7b },
        { "i64x2", 0x7b },     { "f32x4", 0x7b },  { "f64x2", 0x7b }, { "funcref", 0x70 },
        { "externref", 0x6f }, { "exnref", 0x69 },
    };
    for (const Name &n : NAMES)
        if (n.name == s) {
            t = n.type;
            return true;
        }
    return false;
}

// A block's single result, or void.
bool block_type(Str s, u8 &t)
{
    if (s == "void") {
        t = 0x40;
        return true;
    }
    if (s == "i8x16" || s == "i16x8" || s == "i32x4" || s == "i64x2" || s == "f32x4" ||
        s == "f64x2")
        return false;
    return value_type(s, t);
}

bool equal_nocase(Str a, Str b)
{
    if (a.size() != b.size())
        return false;
    for (usize i = 0; i < a.size(); i++)
        if ((a[i] | 0x20) != (b[i] | 0x20))
            return false;
    return true;
}

// A symbol and a constant: what an expression comes to here.
struct Value {
    bool has_sym = false;
    u32 sym      = 0;
    i64 add      = 0;
};

enum OpKind : u8 { O_INT, O_FLT, O_SYM, O_BRLIST, O_TYPES, O_BLOCKTYPE, O_TYPEINDEX };

struct Operand {
    u8 kind  = O_INT;
    u32 at   = 0;
    i64 i    = 0; // an integer, or a block type's byte
    u64 bits = 0; // a float, as a double's bits
    Value v;      // a symbol and addend; a type index's temp symbol
};

void put_i64(Out &o, i64 v)
{
    u64 m = v < 0 ? u64(0) - u64(v) : u64(v);
    char t[20];
    u32 k = 0;
    do {
        t[k++] = char('0' + m % 10);
        m /= 10;
    } while (m);
    if (v < 0)
        o.put('-');
    while (k)
        o.put(t[--k]);
}

struct Parser {
    Asm &a;
    Diag &diag;
    Str name;
    Str src;
    Lexer lx;
    Token t;
    u32 cur     = 0;
    u8 state    = FILE_START;
    bool failed = false;
    Vec<u8> nest;
    Vec<Operand> ops;
    Vec<u32> brs;   // br_table's depths
    Vec<u8> types;  // select's types
    String scratch; // a decoded string

    Parser(Asm &asm_, Diag &d, Str n, Str s) : a(asm_), diag(d), name(n), src(s), lx(s) {}

    // -------------------------------------------------------- tokens, errors

    // The next token. A lexical error is reported when the parser takes the
    // bad token, as AsmParser::Lex does, and not when it stops at it.
    void lex()
    {
        if (t.kind == Tok::Error)
            err(t.at, t.why);
        t = lx.next();
    }

    // Past the rest of a refused statement, saying nothing more.
    void skip_statement()
    {
        while (!is_end())
            t = lx.next();
    }

    Str text(const Token &k) const { return lx.text(k); }

    // An error at `at`; false, so that a statement can return it.
    bool err(u32 at, Str msg)
    {
        report(diag, name, src, at, "error", msg);
        failed = true;
        return false;
    }

    // An error that ends with the current token's text, at that token.
    bool err_got(Str msg)
    {
        Out m;
        m.put(msg).put(text(t));
        return err(t.at, m.str());
    }

    void warn(u32 at, Str msg) { report(diag, name, src, at, "warning", msg); }

    bool is(Tok k) const { return t.kind == k; }

    bool is_end() const { return t.kind == Tok::Eol || t.kind == Tok::Eof; }

    // The current token if it is a `k`, and true.
    bool next_is(Tok k)
    {
        if (t.kind != k)
            return false;
        lex();
        return true;
    }

    bool expect(Tok k, Str what)
    {
        if (next_is(k))
            return true;
        Out m;
        m.put("Expected ").put(what).put(", instead got: ");
        return err_got(m.str());
    }

    bool eol(Str what)
    {
        if (is_end())
            return true;
        return expect(Tok::Eol, what);
    }

    bool expect_ident(Str &s)
    {
        if (!is(Tok::Ident))
            return err_got("Expected identifier, got: ");
        s = text(t);
        lex();
        return true;
    }

    // A string's contents as they are written, escapes and all, as
    // getStringContents gives them; or an identifier.
    bool expect_string_or_ident(Str &s)
    {
        if (is(Tok::String)) {
            s = text(t).substr(1, t.len - 2);
            lex();
            return true;
        }
        if (is(Tok::Ident)) {
            s = text(t);
            lex();
            return true;
        }
        return err_got("Expected string or identifier, got: ");
    }

    // A string token's bytes into `scratch`, escapes decoded as
    // AsmParser::parseEscapedString decodes them.
    bool decode(const Token &k)
    {
        scratch.clear();
        Str s = text(k).substr(1, k.len - 2);
        for (usize i = 0; i < s.size(); i++) {
            char c = s[i];
            if (c != '\\') {
                if (!scratch.push(c))
                    a.oom = true;
                continue;
            }
            if (++i == s.size())
                return err(k.at, "unexpected backslash at end of string");
            c     = s[i];
            u32 v = 0;
            if (c == 'x' || c == 'X') {
                if (i + 1 == s.size() || !((s[i + 1] >= '0' && s[i + 1] <= '9') ||
                                           ((s[i + 1] | 0x20) >= 'a' && (s[i + 1] | 0x20) <= 'f')))
                    return err(k.at, "invalid hexadecimal escape sequence");
                while (i + 1 < s.size()) {
                    char h = s[i + 1];
                    u32 d;
                    if (h >= '0' && h <= '9')
                        d = u32(h - '0');
                    else if ((h | 0x20) >= 'a' && (h | 0x20) <= 'f')
                        d = u32((h | 0x20) - 'a' + 10);
                    else
                        break;
                    v = v * 16 + d;
                    i++;
                }
            } else if (c >= '0' && c <= '7') {
                v = u32(c - '0');
                for (u32 n = 1; n < 3 && i + 1 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '7';
                     n++)
                    v = v * 8 + u32(s[++i] - '0');
                if (v > 255)
                    return err(k.at, "invalid octal escape sequence (out of range)");
            } else {
                switch (c) {
                case 'b':
                    v = '\b';
                    break;
                case 'f':
                    v = '\f';
                    break;
                case 'n':
                    v = '\n';
                    break;
                case 'r':
                    v = '\r';
                    break;
                case 't':
                    v = '\t';
                    break;
                case '"':
                    v = '"';
                    break;
                case '\\':
                    v = '\\';
                    break;
                default:
                    return err(k.at, "invalid escape sequence (unrecognized character)");
                }
            }
            if (!scratch.push(char(v)))
                a.oom = true;
        }
        return true;
    }

    // ------------------------------------------------------------ sections

    Section &section() { return a.sections[cur]; }

    // The section of that name, made with `kind` and `flags` when new.
    u32 find_section(Str sname, u8 kind, u32 flags)
    {
        if (const u32 *k = a.section_by_name.find(sname))
            return *k;
        u32 k = u32(a.sections.size());
        Section s;
        s.name  = sname;
        s.kind  = kind;
        s.flags = flags;
        if (!a.sections.push(move(s)) || !a.section_by_name.insert(sname, k))
            a.oom = true;
        return k;
    }

    void bytes(Bytes b)
    {
        Emit e{ section().bytes };
        e.bytes(b);
        a.oom |= e.oom;
    }

    void byte(u8 b)
    {
        if (!section().bytes.push(b))
            a.oom = true;
    }

    void uleb(u64 v)
    {
        Emit e{ section().bytes };
        e.uleb64(v);
        a.oom |= e.oom;
    }

    void sleb(i64 v)
    {
        Emit e{ section().bytes };
        e.sleb64(v);
        a.oom |= e.oom;
    }

    // A fixup at the end of the section, and the zeros it will fill.
    void fixup(const Value &v, i64 add, u8 kind)
    {
        Section &s = section();
        if (!s.fixups.push(Fixup{ u32(s.bytes.size()), v.sym, add, kind }))
            a.oom = true;
        Emit e{ s.bytes };
        switch (kind) {
        case F_SLEB32:
            e.sleb5(0);
            break;
        case F_SLEB64:
            e.sleb10(0);
            break;
        case F_ULEB64:
            e.uleb10(0);
            break;
        case F_DATA1:
            e.byte(0);
            break;
        case F_DATA2:
            e.byte(0);
            e.byte(0);
            break;
        case F_DATA4:
            e.u32le(0);
            break;
        case F_DATA8:
            e.u32le(0);
            e.u32le(0);
            break;
        default:
            e.uleb5(0);
            break;
        }
        a.oom |= e.oom;
    }

    // ------------------------------------------------------------ nesting

    static Str begin_name(u8 n)
    {
        static constexpr Str NAMES[] = { "function", "block", "loop", "if", "else" };
        return NAMES[n];
    }

    static Str end_name(u8 n)
    {
        static constexpr Str NAMES[] = { "end_function", "end_block", "end_loop", "end_if",
                                         "end_if" };
        return NAMES[n];
    }

    bool pop(Str ins, u8 n1, u8 n2)
    {
        if (nest.empty()) {
            Out m;
            m.put("End of block construct with no start: ").put(ins);
            return err(t.at, m.str());
        }
        u8 top = nest.back();
        if (top != n1 && top != n2) {
            Out m;
            m.put("Block construct type mismatch, expected: ")
                .put(end_name(top))
                .put(", instead got: ")
                .put(ins);
            return err(t.at, m.str());
        }
        nest.pop();
        return true;
    }

    // False, with an error for each, when blocks are left open.
    bool ensure_empty_nesting(u32 at)
    {
        bool ok = nest.empty();
        while (!nest.empty()) {
            Out m;
            m.put("Unmatched block construct(s) at function end: ").put(begin_name(nest.back()));
            err(at, m.str());
            nest.pop();
        }
        return ok;
    }

    void push(u8 n)
    {
        if (!nest.push(n))
            a.oom = true;
    }

    // ------------------------------------------------------------ expressions

    bool primary(Value &v)
    {
        // A bad token was the lexer's to report; AsmParser says no more.
        if (is(Tok::Error))
            return false;
        if (is(Tok::Ident)) {
            Token id = t;
            lex();
            if (is(Tok::At)) {
                lex();
                Out m;
                m.put("invalid variant '").put(is(Tok::Ident) ? text(t) : Str()).put("'");
                return err(id.at, m.str());
            }
            v.has_sym = true;
            v.sym     = a.symbol(text(id));
            v.add     = 0;
            return true;
        }
        if (is(Tok::Integer)) {
            v     = Value{};
            v.add = i64(t.value);
            lex();
            return true;
        }
        if (is(Tok::Minus)) {
            u32 at = t.at;
            lex();
            if (!primary(v))
                return false;
            if (v.has_sym)
                return err(at, "expected relocatable expression");
            v.add = i64(u64(0) - u64(v.add));
            return true;
        }
        if (is(Tok::LParen)) {
            lex();
            if (!expression(v))
                return false;
            return expect(Tok::RParen, ")");
        }
        return err(t.at, "unknown token in expression");
    }

    // primary (('+' | '-') primary)*: a symbol plus or minus constants.
    bool expression(Value &v)
    {
        u32 at = t.at;
        if (!primary(v))
            return false;
        while (is(Tok::Plus) || is(Tok::Minus)) {
            bool minus = is(Tok::Minus);
            lex();
            Value r;
            if (!primary(r))
                return false;
            if (r.has_sym && (minus || v.has_sym))
                return err(at, "expected relocatable expression");
            if (r.has_sym) {
                v.has_sym = true;
                v.sym     = r.sym;
            }
            v.add = minus ? i64(u64(v.add) - u64(r.add)) : i64(u64(v.add) + u64(r.add));
        }
        return true;
    }

    bool absolute(u64 &n)
    {
        u32 at = t.at;
        Value v;
        if (!expression(v))
            return false;
        if (v.has_sym)
            return err(at, "expected absolute expression");
        n = u64(v.add);
        return true;
    }

    // ------------------------------------------------------------ statements

    bool run()
    {
        a.table          = a.symbol("__indirect_function_table");
        Symbol &table    = a.syms[a.table];
        table.kind       = K_TABLE;
        table.table_elem = 0x70;
        cur              = find_section(".text", S_TEXT, 0);
        lex();
        while (!is(Tok::Eof)) {
            if (is(Tok::Eol)) {
                lex();
                continue;
            }
            if (!statement())
                skip_statement();
        }
        ensure_empty_nesting(t.at);
        return !failed && !a.oom;
    }

    bool statement()
    {
        if (is(Tok::Ident) || is(Tok::String)) {
            if (lx.peek().kind == Tok::Colon)
                return label();
            if (is(Tok::Ident) && text(t).starts_with("."))
                return directive();
            if (is(Tok::Ident))
                return instruction();
        }
        return err(t.at, "unexpected token at start of statement");
    }

    bool label()
    {
        Token id = t;
        lex(); // the name
        lex(); // the colon
        Str sname = id.kind == Tok::String ? text(id).substr(1, id.len - 2) : text(id);
        u32 s     = a.symbol(sname);
        if (a.syms[s].defined) {
            Out m;
            m.put("symbol '").put(sname).put("' is already defined");
            return err(id.at, m.str());
        }
        before_label(s, id.at);
        a.enlist(s);
        Symbol &y = a.syms[s];
        y.defined = true;
        y.section = cur;
        y.offset  = u32(section().bytes.size());
        return true;
    }

    // doBeforeLabelEmit: in code, a label begins a function, in a section of
    // its own.
    void before_label(u32 s, u32 at)
    {
        if (section().kind != S_TEXT)
            return;
        if (a.syms[s].kind == K_DATA) {
            err(at, "Wasm doesn't support data symbols in text sections");
            return;
        }
        Str sname = a.syms[s].name;
        if (sname.starts_with(".L"))
            return;
        String full;
        if (!full.append(".text.") || !full.append(sname))
            a.oom = true;
        Str kept = full.str();
        if (!a.section_by_name.find(kept))
            kept = a.keep(kept);
        cur = find_section(kept, S_TEXT, 0);
        if (a.syms[s].kind == K_FUNCTION) {
            ensure_empty_nesting(at);
            state = FUNCTION_LABEL;
            push(N_FUNCTION);
        }
    }

    // A function's locals go before its first instruction, none if no
    // .local said otherwise.
    void ensure_locals()
    {
        if (state == FUNCTION_START) {
            byte(0);
            state = FUNCTION_LOCALS;
        }
    }

    bool check_data_section()
    {
        if (state != DATA_SECTION && section().kind == S_TEXT)
            return err_got("data directive must occur in a data segment: ");
        state = DATA_SECTION;
        return true;
    }

    bool type_list(Vec<u8> &out)
    {
        while (is(Tok::Ident)) {
            u8 ty;
            if (!value_type(text(t), ty))
                return err_got("unknown type: ");
            if (!out.push(ty))
                a.oom = true;
            lex();
            if (!next_is(Tok::Comma))
                break;
        }
        return true;
    }

    // (params) -> (results), into a new signature.
    bool signature(i32 &k)
    {
        Sig sig;
        if (!expect(Tok::LParen, "(") || !type_list(sig.params) || !expect(Tok::RParen, ")") ||
            !expect(Tok::Arrow, "->") || !expect(Tok::LParen, "(") || !type_list(sig.results) ||
            !expect(Tok::RParen, ")"))
            return false;
        k = i32(a.sigs.size());
        if (!a.sigs.push(move(sig)))
            a.oom = true;
        return true;
    }

    bool directive()
    {
        Token id = t;
        Str d    = text(id);
        lex();

        // WebAssemblyAsmParser's own, which it sees first.
        if (d == ".globaltype")
            return globaltype();
        if (d == ".tabletype")
            return tabletype();
        if (d == ".functype")
            return functype();
        if (d == ".export_name" || d == ".import_module" || d == ".import_name")
            return name_directive(d);
        if (d == ".local")
            return locals();
        if (d == ".int8" || d == ".int16" || d == ".int32" || d == ".int64") {
            if (!check_data_section())
                return false;
            Value v;
            if (!expression(v))
                return err_got("Cannot parse .int expression: ");
            u32 size = d == ".int8" ? 1 : d == ".int16" ? 2 : d == ".int32" ? 4 : 8;
            return value(v, size, t.at) && eol("EOL");
        }
        if (d == ".asciz") {
            if (!check_data_section())
                return false;
            return strings(true) && eol("EOL");
        }

        // WasmAsmParser's and AsmParser's.
        if (d == ".text")
            return eol("eol");
        if (d == ".data") {
            cur = find_section(".data", S_DATA, 0);
            return eol("eol");
        }
        if (d == ".section")
            return section_directive(id.at);
        if (d == ".size")
            return size_directive(id.at);
        if (d == ".type")
            return type_directive();
        if (d == ".globl" || d == ".global" || d == ".weak" || d == ".hidden")
            return attribute(d);
        if (d == ".ident") {
            if (!is(Tok::String))
                return err(t.at, "unexpected token in '.ident' directive");
            lex();
            if (!is_end())
                return err(t.at, "unexpected token in '.ident' directive");
            return true;
        }
        if (d == ".file") {
            if (is(Tok::Integer))
                lex();
            while (is(Tok::String))
                lex();
            return eol("eol");
        }
        if (d == ".ascii")
            return strings(false) && eol("eol");
        if (d == ".string")
            return strings(true) && eol("eol");
        if (d == ".skip" || d == ".space")
            return skip();
        if (d == ".p2align" || d == ".align")
            return align(true);
        if (d == ".balign")
            return align(false);
        return err(id.at, "unknown directive");
    }

    bool globaltype()
    {
        Str sname, tname;
        if (!expect_ident(sname) || !expect(Tok::Comma, ","))
            return false;
        Token tt = t;
        if (!expect_ident(tname))
            return false;
        u8 ty;
        if (!value_type(tname, ty)) {
            Out m;
            m.put("Unknown type in .globaltype directive: ").put(text(tt));
            return err(tt.at, m.str());
        }
        bool mut = true;
        if (next_is(Tok::Comma)) {
            Token mt = t;
            Str mod;
            if (!expect_ident(mod))
                return false;
            if (mod != "immutable") {
                Out m;
                m.put("Unknown type in .globaltype modifier: ").put(text(mt));
                return err(mt.at, m.str());
            }
            mut = false;
        }
        Symbol &y        = a.syms[a.symbol(sname)];
        y.kind           = K_GLOBAL;
        y.global_type    = ty;
        y.global_mutable = mut;
        return eol("EOL");
    }

    bool tabletype()
    {
        Str sname, ename;
        if (!expect_ident(sname) || !expect(Tok::Comma, ","))
            return false;
        Token et = t;
        if (!expect_ident(ename))
            return false;
        u8 ty;
        if (!value_type(ename, ty)) {
            Out m;
            m.put("Unknown type in .tabletype directive: ").put(text(et));
            return err(et.at, m.str());
        }
        u32 flags = 0, min = 0, max = 0;
        if (next_is(Tok::Comma)) {
            if (!is(Tok::Integer))
                return err_got("Expected integer constant, instead got: ");
            min = u32(t.value);
            lex();
            if (next_is(Tok::Comma)) {
                if (!is(Tok::Integer))
                    return err_got("Expected integer constant, instead got: ");
                flags |= wasm::LIMITS_MAX;
                max = u32(t.value);
                lex();
            }
        }
        Symbol &y     = a.syms[a.symbol(sname)];
        y.kind        = K_TABLE;
        y.table_elem  = ty;
        y.table_flags = flags;
        y.table_min   = min;
        y.table_max   = max;
        return eol("EOL");
    }

    bool functype()
    {
        Str sname;
        if (!expect_ident(sname))
            return false;
        u32 s = a.symbol(sname);
        if (a.syms[s].defined) {
            // A function begins at its .functype when no label said so.
            if (state != FUNCTION_LABEL) {
                if (!ensure_empty_nesting(t.at))
                    return false;
                push(N_FUNCTION);
            }
            state = FUNCTION_START;
        }
        i32 sig;
        if (!signature(sig))
            return false;
        a.syms[s].sig  = sig;
        a.syms[s].kind = K_FUNCTION;
        return eol("EOL");
    }

    bool name_directive(Str d)
    {
        Str sname, value;
        if (!expect_ident(sname) || !expect(Tok::Comma, ",") || !expect_string_or_ident(value))
            return false;
        Symbol &y = a.syms[a.symbol(sname)];
        if (d == ".export_name") {
            y.export_name = value;
            y.has_export  = true;
        } else if (d == ".import_module") {
            y.import_module = value;
            y.has_module    = true;
        } else {
            y.import_name = value;
            y.has_import  = true;
        }
        return eol("EOL");
    }

    bool locals()
    {
        if (state != FUNCTION_START)
            return err_got(".local directive should follow the start of a function: ");
        Vec<u8> list;
        if (!type_list(list))
            return false;
        // As runs of one type: how many runs, then each run's length and type.
        u32 runs = 0;
        for (usize i = 0; i < list.size(); i++)
            if (i == 0 || list[i] != list[i - 1])
                runs++;
        uleb(runs);
        for (usize i = 0; i < list.size();) {
            usize j = i;
            while (j < list.size() && list[j] == list[i])
                j++;
            uleb(j - i);
            byte(list[i]);
            i = j;
        }
        state = FUNCTION_LOCALS;
        return eol("EOL");
    }

    // .int8 to .int64: a constant, or a symbol plus one, which a relocation
    // fills.
    bool value(const Value &v, u32 size, u32 at)
    {
        if (v.has_sym) {
            a.enlist(v.sym);
            static constexpr u8 KINDS[] = { 0, F_DATA1, F_DATA2, 0, F_DATA4, 0, 0, 0, F_DATA8 };
            fixup(v, v.add, KINDS[size]);
            return true;
        }
        if (size < 8) {
            i64 lo = -(i64(1) << (8 * size - 1));
            i64 hi = (i64(1) << (8 * size)) - 1;
            if (v.add < lo || v.add > hi) {
                Out m;
                m.put("value evaluated as ");
                put_i64(m, v.add);
                m.put(" is out of range.");
                return err(at, m.str());
            }
        }
        for (u32 k = 0; k < size; k++)
            byte(u8(u64(v.add) >> (8 * k)));
        return true;
    }

    // One or more strings; with a NUL after each for .asciz and .string.
    bool strings(bool zero)
    {
        for (;;) {
            if (!is(Tok::String))
                return err(t.at, "expected string");
            Token s = t;
            lex();
            if (!decode(s))
                return false;
            bytes(Bytes(reinterpret_cast<const u8 *>(scratch.data()), scratch.size()));
            if (zero)
                byte(0);
            if (!next_is(Tok::Comma))
                return true;
        }
    }

    bool skip()
    {
        u64 n;
        if (!absolute(n))
            return false;
        u64 fill = 0;
        if (next_is(Tok::Comma) && !absolute(fill))
            return false;
        for (u64 k = 0; k < n; k++)
            byte(u8(fill));
        return eol("eol");
    }

    // .p2align k[, fill[, max]], or .balign bytes[, fill[, max]].
    bool align(bool pow2)
    {
        u32 at = t.at;
        u64 n;
        if (!absolute(n))
            return false;
        u64 fill = 0, max = 0;
        if (next_is(Tok::Comma)) {
            if (!is(Tok::Comma) && !absolute(fill))
                return false;
            if (next_is(Tok::Comma) && !absolute(max))
                return false;
        }
        u32 log2 = 0;
        if (pow2) {
            if (n >= 32)
                return err(at, "invalid alignment value");
            log2 = u32(n);
        } else {
            if (n == 0)
                n = 1;
            if (n & (n - 1))
                return err(at, "alignment must be a power of 2");
            while ((u64(1) << log2) < n)
                log2++;
        }
        Section &s = section();
        if (log2 > s.align)
            s.align = log2;
        u64 size = s.bytes.size();
        u64 pad  = ((size + (u64(1) << log2) - 1) & ~((u64(1) << log2) - 1)) - size;
        if (max == 0 || pad <= max)
            for (u64 k = 0; k < pad; k++)
                byte(u8(fill));
        return eol("eol");
    }

    bool section_directive(u32 at)
    {
        Str sname;
        if (is(Tok::Ident))
            sname = text(t);
        else if (is(Tok::String))
            sname = text(t).substr(1, t.len - 2);
        else
            return err(t.at, "expected identifier in directive");
        lex();
        if (!expect(Tok::Comma, ","))
            return false;
        if (!is(Tok::String))
            return err_got("expected string in directive, instead got: ");
        Str fl       = text(t).substr(1, t.len - 2);
        u32 flags    = 0;
        bool passive = false;
        for (usize i = 0; i < fl.size(); i++) {
            switch (fl[i]) {
            case 'p':
                passive = true;
                break;
            case 'T':
                flags |= wasm::SEG_TLS;
                break;
            case 'S':
                flags |= wasm::SEG_STRINGS;
                break;
            case 'R':
                flags |= wasm::SEG_RETAIN;
                break;
            case 'G':
                return err(t.at, "comdat groups are not supported");
            default:
                return err(t.at, "unknown flag");
            }
        }
        lex();
        if (!expect(Tok::Comma, ",") || !expect(Tok::At, "@") || !eol("eol"))
            return false;
        u8 kind = sname.starts_with(".text")             ? S_TEXT
                  : sname.starts_with(".custom_section") ? S_META
                  : sname.starts_with(".debug_")         ? S_META
                                                         : S_DATA;
        u32 k   = find_section(sname, kind, flags);
        if (a.sections[k].flags != flags) {
            Out m;
            m.put("changed section flags for ").put(sname).put(", expected: 0x");
            u32 f = a.sections[k].flags;
            char h[8];
            u32 n = 0;
            do {
                h[n++] = "0123456789ABCDEF"[f & 0xf];
                f >>= 4;
            } while (f);
            while (n)
                m.put(h[--n]);
            err(at, m.str());
        }
        if (passive) {
            if (a.sections[k].kind != S_DATA)
                return err(at, "Only data sections can be passive");
            a.sections[k].passive = true;
        }
        cur = k;
        return true;
    }

    bool size_directive(u32 at)
    {
        if (!is(Tok::Ident))
            return err(t.at, "expected identifier in directive");
        u32 s = a.symbol(text(t));
        lex();
        if (!expect(Tok::Comma, ","))
            return false;
        u64 n;
        if (!absolute(n) || !eol("eol"))
            return false;
        Symbol &y = a.syms[s];
        if (y.kind == K_FUNCTION) {
            warn(at, ".size directive ignored for function symbols");
        } else {
            y.size     = n;
            y.has_size = true;
        }
        return true;
    }

    bool type_directive()
    {
        if (!is(Tok::Ident))
            return err_got("Expected label after .type directive, got: ");
        u32 s = a.symbol(text(t));
        lex();
        if (!(next_is(Tok::Comma) && next_is(Tok::At) && is(Tok::Ident)))
            return err_got("Expected label,@type declaration, got: ");
        Str ty = text(t);
        if (ty == "function")
            a.syms[s].kind = K_FUNCTION;
        else if (ty == "global")
            a.syms[s].kind = K_GLOBAL;
        else if (ty == "object")
            a.syms[s].kind = K_DATA;
        else
            return err_got("Unknown WASM symbol type: ");
        lex();
        return eol("EOL");
    }

    // .globl and its kin: each symbol named is registered.
    bool attribute(Str d)
    {
        if (is_end())
            return true;
        for (;;) {
            if (!is(Tok::Ident))
                return err(t.at, "expected identifier in directive");
            u32 s = a.symbol(text(t));
            lex();
            a.enlist(s);
            Symbol &y = a.syms[s];
            if (d == ".weak") {
                y.weak     = true;
                y.external = true;
            } else if (d == ".hidden") {
                y.hidden = true;
            } else {
                y.external = true;
            }
            if (is_end())
                return true;
            if (!is(Tok::Comma))
                return err(t.at, "unexpected token in directive");
            lex();
        }
    }

    // ------------------------------------------------------------ instructions

    bool add_operand(const Operand &o)
    {
        if (!ops.push(o))
            a.oom = true;
        return true;
    }

    // A load's or store's alignment: :p2align=k after its offset, or the
    // default, which the match makes natural.
    bool p2align(Str iname)
    {
        if (!iname.contains(".load") && !iname.contains(".store") && !iname.contains("prefetch"))
            return true;
        Operand o;
        o.at = t.at;
        o.i  = -1;
        if (next_is(Tok::Colon)) {
            Str id;
            if (!expect_ident(id))
                return false;
            if (id != "p2align") {
                Out m;
                m.put("Expected p2align, instead got: ").put(id);
                return err(t.at, m.str());
            }
            if (!expect(Tok::Equal, "="))
                return false;
            if (!is(Tok::Integer))
                return err(t.at, "Expected integer constant");
            o.at = t.at;
            o.i  = i64(t.value);
            lex();
        }
        return add_operand(o);
    }

    // infinity or nan, any case, as a float.
    bool special_float(bool negative)
    {
        if (!is(Tok::Ident))
            return false;
        Str s = text(t);
        u64 bits;
        if (equal_nocase(s, "infinity"))
            bits = 0x7ff0000000000000ull;
        else if (equal_nocase(s, "nan"))
            bits = 0x7ff8000000000000ull;
        else
            return false;
        Operand o;
        o.kind = O_FLT;
        o.at   = t.at;
        o.bits = negative ? bits ^ (u64(1) << 63) : bits;
        add_operand(o);
        lex();
        return true;
    }

    bool real(bool negative, u32 at)
    {
        // llvm takes a real only when a double holds it exactly.
        u64 bits;
        bool exact;
        if (!parse_f64(text(t), bits, &exact) || !exact)
            return err_got("Cannot parse real: ");
        Operand o;
        o.kind = O_FLT;
        o.at   = at;
        o.bits = negative ? bits ^ (u64(1) << 63) : bits;
        add_operand(o);
        lex();
        return true;
    }

    bool instruction()
    {
        Token nm  = t;
        Str iname = text(nm);
        lex();
        ops.clear();
        brs.clear();
        types.clear();

        bool expect_block = false, expect_functype = false;
        Operand table;
        bool has_table = false;
        if (iname == "block") {
            push(N_BLOCK);
            expect_block = true;
        } else if (iname == "loop") {
            push(N_LOOP);
            expect_block = true;
        } else if (iname == "if") {
            push(N_IF);
            expect_block = true;
        } else if (iname == "else") {
            if (!pop(iname, N_IF, N_IF))
                return false;
            push(N_ELSE);
        } else if (iname == "end_if") {
            if (!pop(iname, N_IF, N_ELSE))
                return false;
        } else if (iname == "end_loop") {
            if (!pop(iname, N_LOOP, N_LOOP))
                return false;
        } else if (iname == "end_block") {
            if (!pop(iname, N_BLOCK, N_BLOCK))
                return false;
        } else if (iname == "end_function") {
            ensure_locals();
            state = END_FUNCTION;
            if (!pop(iname, N_FUNCTION, N_FUNCTION) || !ensure_empty_nesting(t.at))
                return false;
        } else if (iname == "call_indirect") {
            // The table first in the text, last in the operands.
            table.kind = O_SYM;
            table.at   = nm.at;
            if (is(Tok::Ident)) {
                u32 s = a.symbol(text(t));
                if (a.syms[s].kind != K_TABLE || a.syms[s].table_elem != 0x70)
                    err(t.at, "symbol is not a wasm funcref table");
                table.v.has_sym = true;
                table.v.sym     = s;
                table.at        = t.at;
                lex();
                if (!expect(Tok::Comma, ","))
                    return false;
            } else {
                table.v.has_sym = true;
                table.v.sym     = a.table;
            }
            has_table       = true;
            expect_functype = true;
        } else if (iname == "select") {
            Operand o;
            o.kind = O_TYPES;
            o.at   = t.at;
            while (is(Tok::Ident)) {
                u8 ty;
                if (!value_type(text(t), ty))
                    return err_got("unknown value type in select operand list: ");
                if (!types.push(ty))
                    a.oom = true;
                lex();
            }
            add_operand(o);
        }

        // A signature: call_indirect's, or a block's of several values.
        if (expect_functype || (is(Tok::LParen) && expect_block)) {
            u32 at = t.at;
            i32 sig;
            if (!signature(sig))
                return false;
            u32 s          = a.temp();
            a.syms[s].sig  = sig;
            a.syms[s].kind = K_FUNCTION;
            Operand o;
            o.kind      = O_TYPEINDEX;
            o.at        = at;
            o.v.has_sym = true;
            o.v.sym     = s;
            add_operand(o);
            expect_block = false;
        }

        while (!is_end()) {
            u32 at = t.at;
            switch (t.kind) {
            case Tok::Ident: {
                if (special_float(false))
                    break;
                if (expect_block) {
                    u8 bt;
                    if (!block_type(text(t), bt))
                        return err_got("Unknown block type: ");
                    Operand o;
                    o.kind = O_BLOCKTYPE;
                    o.at   = at;
                    o.i    = bt;
                    add_operand(o);
                    expect_block = false;
                    lex();
                    break;
                }
                Operand o;
                o.kind = O_SYM;
                o.at   = at;
                if (!expression(o.v))
                    return false;
                add_operand(o);
                if (!p2align(iname))
                    return false;
                break;
            }
            case Tok::Minus:
                lex();
                if (is(Tok::Integer)) {
                    Operand o;
                    o.at = at;
                    o.i  = i64(u64(0) - t.value);
                    add_operand(o);
                    lex();
                    if (!p2align(iname))
                        return false;
                } else if (is(Tok::Real)) {
                    if (!real(true, at))
                        return false;
                } else if (!special_float(true)) {
                    return err_got("Expected numeric constant instead got: ");
                }
                break;
            case Tok::Integer: {
                Operand o;
                o.at = at;
                o.i  = i64(t.value);
                add_operand(o);
                lex();
                if (!p2align(iname))
                    return false;
                break;
            }
            case Tok::Real:
                if (!real(false, at))
                    return false;
                break;
            case Tok::LCurly: {
                lex();
                Operand o;
                o.kind = O_BRLIST;
                o.at   = at;
                if (!is(Tok::RCurly))
                    for (;;) {
                        if (!brs.push(u32(t.value)))
                            a.oom = true;
                        expect(Tok::Integer, "integer"); // as llvm, going on
                        if (!next_is(Tok::Comma))
                            break;
                    }
                expect(Tok::RCurly, "}");
                add_operand(o);
                break;
            }
            default:
                return err_got("Unexpected token in operand: ");
            }
            if (!is_end() && !expect(Tok::Comma, ","))
                return false;
        }
        if (expect_block) {
            Operand o;
            o.kind = O_BLOCKTYPE;
            o.at   = nm.at;
            o.i    = 0x40;
            add_operand(o);
        }
        if (has_table)
            add_operand(table);
        return match(nm, iname);
    }

    // The operands against the instruction's form, and the bytes.
    bool match(const Token &nm, Str iname)
    {
        Instr in;
        if (!find_instr(iname, in))
            return err(nm.at, "invalid instruction");

        // How many operands, and of what kind, each form takes.
        static constexpr u8 ANY_INT = 0xff;
        u8 want[2]                  = { 0, 0 };
        usize n                     = 0;
        switch (in.form) {
        case NONE:
            break;
        case BLOCK:
            n       = 1;
            want[0] = O_BLOCKTYPE;
            break;
        case LABEL:
        case LOCAL:
        case MEMIDX:
            n       = 1;
            want[0] = O_INT;
            break;
        case MEMIDX2:
            n       = 2;
            want[0] = O_INT;
            want[1] = O_INT;
            break;
        case BR_TABLE:
            n       = 1;
            want[0] = O_BRLIST;
            break;
        case GLOBAL:
        case CALL:
        case I32:
        case I64:
            n       = 1;
            want[0] = ANY_INT;
            break;
        case CALL_INDIRECT:
            n       = 2;
            want[0] = O_TYPEINDEX;
            want[1] = O_SYM;
            break;
        case MEM:
            n       = 2;
            want[0] = ANY_INT;
            want[1] = O_INT;
            break;
        case F32:
        case F64:
            n       = 1;
            want[0] = O_FLT;
            break;
        case SELECT:
            n       = 1;
            want[0] = O_TYPES;
            break;
        }
        for (usize k = 0; k < ops.size() && k < n; k++) {
            u8 got  = ops[k].kind;
            bool ok = want[k] == ANY_INT       ? (got == O_INT || got == O_SYM)
                      : want[k] == O_BLOCKTYPE ? (got == O_BLOCKTYPE || got == O_TYPEINDEX)
                                               : got == want[k];
            if (!ok)
                return err(ops[k].at, "invalid operand for instruction");
        }
        if (ops.size() < n)
            return err(nm.at, "too few operands for instruction");
        if (ops.size() > n)
            return err(ops[n].at, "invalid operand for instruction");
        // What llvm's type checker says of a call or a global.
        if (in.form == GLOBAL || in.form == CALL) {
            const Operand &o = ops[0];
            if (o.kind == O_INT) {
                // The code emitter says so at once, the type checker after.
                if (in.form == GLOBAL) {
                    diag.text.put(
                        "<unknown>:0: error: Wasm globals should only be accessed symbolically!\n");
                    diag.errors++;
                }
                return err(o.at, "expected expression operand");
            }
            if (o.v.add)
                return err(o.at, "expected symbol operand");
            const Symbol &y = a.syms[o.v.sym];
            bool typed =
                in.form == GLOBAL ? y.kind == K_GLOBAL : y.kind == K_FUNCTION && y.sig >= 0;
            if (!typed) {
                Out m;
                m.put("symbol ").put(y.name).put(in.form == GLOBAL ? Str(": missing .globaltype")
                                                                   : Str(": missing .functype"));
                return err(o.at, m.str());
            }
        }
        if (in.form == LABEL || in.form == LOCAL || in.form == MEMIDX || in.form == MEMIDX2)
            for (const Operand &o : ops)
                if (o.kind == O_SYM)
                    return err(o.at, "invalid operand for instruction");

        ensure_locals();
        // A select with types is the typed one. llvm writes that one even with
        // no types, which is not valid; this writes the plain select.
        if (in.form == SELECT && !types.empty())
            in.sub = 0x1c;
        if (in.prefix) {
            byte(in.prefix);
            uleb(in.sub);
        } else {
            byte(u8(in.sub));
        }
        switch (in.form) {
        case NONE:
            break;
        case BLOCK:
            if (ops[0].kind == O_TYPEINDEX) {
                a.enlist(ops[0].v.sym);
                fixup(ops[0].v, 0, F_TYPEINDEX);
            } else {
                byte(u8(ops[0].i));
            }
            break;
        case LABEL:
        case LOCAL:
        case MEMIDX:
            uleb(u32(ops[0].i));
            break;
        case MEMIDX2:
            uleb(u32(ops[0].i));
            uleb(u32(ops[1].i));
            break;
        case BR_TABLE:
            uleb(brs.size() - 1);
            for (u32 b : brs)
                uleb(b);
            break;
        case GLOBAL:
        case CALL:
            a.enlist(ops[0].v.sym);
            fixup(ops[0].v, 0, F_ULEB32);
            break;
        case CALL_INDIRECT:
            // Operands are visited last to first.
            a.enlist(ops[1].v.sym);
            a.enlist(ops[0].v.sym);
            fixup(ops[0].v, 0, F_TYPEINDEX);
            fixup(ops[1].v, 0, F_ULEB32);
            break;
        case MEM:
            uleb(ops[1].i == -1 ? in.align : u64(ops[1].i));
            if (ops[0].kind == O_SYM) {
                a.enlist(ops[0].v.sym);
                fixup(ops[0].v, ops[0].v.add, F_ULEB32);
            } else {
                uleb(u32(ops[0].i));
            }
            break;
        case I32:
            if (ops[0].kind == O_SYM) {
                a.enlist(ops[0].v.sym);
                fixup(ops[0].v, ops[0].v.add, F_SLEB32);
            } else {
                sleb(i32(ops[0].i));
            }
            break;
        case I64:
            if (ops[0].kind == O_SYM) {
                a.enlist(ops[0].v.sym);
                fixup(ops[0].v, ops[0].v.add, F_SLEB64);
            } else {
                sleb(ops[0].i);
            }
            break;
        case F32: {
            u32 b = f32_bits(ops[0].bits);
            for (u32 k = 0; k < 4; k++)
                byte(u8(b >> (8 * k)));
            break;
        }
        case F64:
            for (u32 k = 0; k < 8; k++)
                byte(u8(ops[0].bits >> (8 * k)));
            break;
        case SELECT:
            if (types.empty())
                break;
            uleb(types.size());
            for (u8 ty : types)
                byte(ty);
            break;
        }
        if (state != END_FUNCTION)
            state = INSTRUCTIONS;
        return true;
    }

    // A double narrowed to a float, as llvm narrows f32.const's operand.
    static u32 f32_bits(u64 d)
    {
        u32 sign = u32(d >> 32) & 0x80000000u;
        if ((d & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) {
            if (d & 0x000fffffffffffffull)
                return sign | 0x7fc00000u | u32((d & 0x0007ffffffffffffull) >> 29);
            return sign | 0x7f800000u;
        }
        double x = __builtin_bit_cast(double, d);
        float f  = float(x);
        return __builtin_bit_cast(u32, f);
    }
};

// The line holding `at`, with tabs expanded as llvm's SourceMgr expands
// them, and the column `at` lands in.
void line_of(Str src, u32 at, Str &line, u32 &lineno, u32 &col)
{
    u32 start = 0;
    lineno    = 1;
    for (u32 k = 0; k < at && k < src.size(); k++)
        if (src[k] == '\n') {
            lineno++;
            start = k + 1;
        }
    u32 end = start;
    while (end < src.size() && src[end] != '\n')
        end++;
    if (end > start && src[end - 1] == '\r')
        end--;
    line = src.substr(start, end - start);
    col  = at - start + 1;
}

} // namespace

void report(Diag &diag, Str name, Str source, u32 at, Str kind, Str msg)
{
    Str line;
    u32 lineno, col;
    line_of(source, at, line, lineno, col);
    Out &o = diag.text;
    o.put(name).put(':').num(lineno).put(':').num(col).put(": ").put(kind).put(": ").put(msg);
    o.put('\n');
    u32 vis = 0, caret = 0;
    for (u32 k = 0; k < line.size(); k++) {
        if (k + 1 == col)
            caret = vis;
        if (line[k] == '\t') {
            do {
                o.put(' ');
                vis++;
            } while (vis % 8);
        } else {
            o.put(line[k]);
            vis++;
        }
    }
    if (col > line.size())
        caret = vis + (col - 1 - line.size());
    o.put('\n');
    for (u32 k = 0; k < caret; k++)
        o.put(' ');
    o.put("^\n");
    if (kind == "error")
        diag.errors++;
}

bool parse(Str name, Str source, Asm &a, Diag &diag)
{
    Parser p(a, diag, name, source);
    return p.run();
}

} // namespace as
