#include "writer.h"

#include "emit.h"
#include "out.h"
#include "wasm.h"

namespace as {

namespace {

constexpr u32 NONE_INDEX = ~u32(0);

struct Reloc {
    u8 type;
    u32 section; // the Asm section it patches
    u32 offset;  // in that section
    u32 sym;
    i64 addend;
};

struct Import {
    Str module;
    Str field;
    u8 kind;
    u32 sig    = 0;
    u8 type    = 0; // a global's value type, or a table's element type
    bool mut   = false;
    u32 tflags = 0;
};

struct Segment {
    u32 section;
    u64 offset; // in linear memory
};

struct SymInfo {
    u8 kind;
    u32 flags;
    u32 sym;
};

// Padded LEBs and words, written over bytes already there.
void patch_uleb5(u8 *p, u32 v)
{
    for (u32 k = 0; k < 4; k++, v >>= 7)
        p[k] = u8((v & 0x7f) | 0x80);
    p[4] = u8(v & 0x7f);
}

void patch_sleb(u8 *p, i64 v, u32 width)
{
    for (u32 k = 0; k + 1 < width; k++, v >>= 7)
        p[k] = u8((v & 0x7f) | 0x80);
    p[width - 1] = u8(v & 0x7f);
}

void patch_le(u8 *p, u64 v, u32 width)
{
    for (u32 k = 0; k < width; k++)
        p[k] = u8(v >> (8 * k));
}

bool same(const Sig &a, const Sig &b)
{
    if (a.params.size() != b.params.size() || a.results.size() != b.results.size())
        return false;
    for (usize k = 0; k < a.params.size(); k++)
        if (a.params[k] != b.params[k])
            return false;
    for (usize k = 0; k < a.results.size(); k++)
        if (a.results[k] != b.results[k])
            return false;
    return true;
}

bool is_table_index(u8 t)
{
    return t == wasm::R_TABLE_INDEX_SLEB || t == wasm::R_TABLE_INDEX_I32 ||
           t == wasm::R_TABLE_INDEX_SLEB64 || t == wasm::R_TABLE_INDEX_I64 ||
           t == wasm::R_TABLE_INDEX_REL_SLEB || t == wasm::R_TABLE_INDEX_REL_SLEB64;
}

struct Writer {
    Asm &a;
    Diag &diag;
    Vec<u8> &out;
    bool failed = false;

    Vec<Reloc> code_relocs, data_relocs;
    Vec<u32> index;       // a function's, global's or table's index
    Vec<u32> type_index;  // a function's type
    Vec<u32> table_index; // an address-taken function's slot, from 1
    Vec<u32> sym_index;   // in the symbol table
    Vec<u32> data_seg, data_off;
    Vec<u64> data_size;
    Vec<u32> sigs; // the distinct signatures, into a.sigs
    Vec<Import> imports;
    Vec<u32> functions; // defined, by symbol
    Vec<u32> globals;
    Vec<u32> tables;
    Vec<Segment> segments;
    Vec<i32> seg_of; // a section's segment, or -1
    Vec<u32> customs;
    i32 producers = -1, features = -1;
    Vec<u32> elems; // the table's functions, by index
    Vec<SymInfo> symtab;
    Vec<u32> sec_offset; // where a section's bytes went in CODE or DATA
    u32 n_func_imports = 0, n_global_imports = 0, n_table_imports = 0;
    u64 memory_size   = 0;
    u32 section_count = 0;

    Writer(Asm &asm_, Diag &d, Vec<u8> &o) : a(asm_), diag(d), out(o) {}

    bool fatal(Str msg)
    {
        diag.error(msg);
        failed = true;
        return false;
    }

    bool fatal_named(Str msg, Str name)
    {
        Out m;
        m.put(msg).put(name);
        return fatal(m.str());
    }

    template <class T>
    void grow(Vec<T> &v, usize n, T fill)
    {
        for (usize k = v.size(); k < n; k++)
            if (!v.push(fill))
                a.oom = true;
    }

    static bool is_data(const Symbol &y) { return y.kind == K_NONE || y.kind == K_DATA; }

    static bool in_symtab(const Symbol &y) { return y.in_reloc || !y.temp; }

    // ------------------------------------------------------------ relocations

    // Each fixup's relocation, in section order, as MCAssembler::layout
    // records them; a table index makes the table live.
    bool record()
    {
        for (u32 k = 0; k < a.sections.size(); k++) {
            Section &s = a.sections[k];
            for (Fixup &f : s.fixups) {
                Symbol &y = a.syms[f.sym];
                u8 t;
                switch (f.kind) {
                case F_TYPEINDEX:
                    t = wasm::R_TYPE_INDEX_LEB;
                    break;
                case F_SLEB32:
                    t = y.kind == K_FUNCTION ? wasm::R_TABLE_INDEX_SLEB : wasm::R_MEMORY_ADDR_SLEB;
                    break;
                case F_SLEB64:
                    t = y.kind == K_FUNCTION ? wasm::R_TABLE_INDEX_SLEB64
                                             : wasm::R_MEMORY_ADDR_SLEB64;
                    break;
                case F_ULEB32:
                    t = y.kind == K_GLOBAL     ? wasm::R_GLOBAL_INDEX_LEB
                        : y.kind == K_FUNCTION ? wasm::R_FUNCTION_INDEX_LEB
                        : y.kind == K_TABLE    ? wasm::R_TABLE_NUMBER_LEB
                                               : wasm::R_MEMORY_ADDR_LEB;
                    break;
                case F_ULEB64:
                    t = wasm::R_MEMORY_ADDR_LEB64;
                    break;
                case F_DATA4:
                    if (s.kind == S_META)
                        return fatal("relocations in custom sections are not supported");
                    t = y.kind == K_FUNCTION ? wasm::R_TABLE_INDEX_I32
                        : y.kind == K_GLOBAL ? wasm::R_GLOBAL_INDEX_I32
                                             : wasm::R_MEMORY_ADDR_I32;
                    break;
                case F_DATA8:
                    if (s.kind == S_META)
                        return fatal("relocations in custom sections are not supported");
                    if (y.kind == K_GLOBAL)
                        return fatal("unimplemented R_WASM_GLOBAL_INDEX_I64");
                    t = y.kind == K_FUNCTION ? wasm::R_TABLE_INDEX_I64 : wasm::R_MEMORY_ADDR_I64;
                    break;
                default:
                    return fatal("unimplemented fixup kind: a symbol in .int8 or .int16");
                }
                if (is_table_index(t)) {
                    a.syms[a.table].no_strip = true;
                    a.enlist(a.table);
                }
                if (t != wasm::R_TYPE_INDEX_LEB) {
                    if (y.name.empty())
                        return fatal(
                            "relocations against un-named temporaries are not yet "
                            "supported by wasm");
                    y.in_reloc = true;
                }
                f.reloc = t;
                Reloc r{ t, k, f.offset, f.sym, f.addend };
                if (s.kind == S_TEXT) {
                    if (!code_relocs.push(r))
                        a.oom = true;
                } else if (s.kind == S_DATA) {
                    if (!data_relocs.push(r))
                        a.oom = true;
                } else {
                    return fatal("relocations in custom sections are not supported");
                }
            }
        }
        return true;
    }

    // ------------------------------------------------------------ symbols

    void register_type(u32 s)
    {
        const Symbol &y = a.syms[s];
        Sig none;
        const Sig &sig = y.sig >= 0 ? a.sigs[u32(y.sig)] : none;
        for (u32 k = 0; k < sigs.size(); k++) {
            const Sig &other = sigs[k] == NONE_INDEX ? none : a.sigs[sigs[k]];
            if (same(sig, other)) {
                type_index[s] = k;
                return;
            }
        }
        type_index[s] = u32(sigs.size());
        if (!sigs.push(y.sig >= 0 ? u32(y.sig) : NONE_INDEX))
            a.oom = true;
    }

    bool imports_of()
    {
        Import mem;
        mem.module = "env";
        mem.field  = "__linear_memory";
        mem.kind   = wasm::EXT_MEMORY;
        if (!imports.push(mem))
            a.oom = true;
        for (u32 s : a.order) {
            Symbol &y = a.syms[s];
            if (y.kind == K_FUNCTION)
                register_type(s);
            if (y.temp || y.defined)
                continue;
            Import im;
            im.module = y.has_module ? y.import_module : Str("env");
            im.field  = y.has_import ? y.import_name : y.name;
            if (y.kind == K_FUNCTION) {
                im.kind  = wasm::EXT_FUNCTION;
                im.sig   = type_index[s];
                index[s] = n_func_imports++;
            } else if (y.kind == K_GLOBAL) {
                if (y.weak)
                    return fatal("undefined global symbol cannot be weak");
                im.kind  = wasm::EXT_GLOBAL;
                im.type  = y.global_type;
                im.mut   = y.global_mutable;
                index[s] = n_global_imports++;
            } else if (y.kind == K_TABLE) {
                if (y.weak)
                    return fatal("undefined table symbol cannot be weak");
                im.kind   = wasm::EXT_TABLE;
                im.type   = y.table_elem;
                im.tflags = y.table_flags;
                index[s]  = n_table_imports++;
            } else {
                continue;
            }
            if (!imports.push(im))
                a.oom = true;
        }
        return true;
    }

    // Data sections become segments, laid out one after another as each
    // one's alignment allows; metadata sections become custom sections.
    void segments_of()
    {
        for (u32 k = 0; k < a.sections.size(); k++) {
            const Section &s = a.sections[k];
            if (s.kind == S_DATA) {
                u64 al      = u64(1) << s.align;
                memory_size = (memory_size + al - 1) & ~(al - 1);
                seg_of[k]   = i32(segments.size());
                if (!segments.push(Segment{ k, memory_size }))
                    a.oom = true;
                memory_size += s.bytes.size();
            } else if (s.kind == S_META) {
                Str n = custom_name(s.name);
                if (n == "producers")
                    producers = i32(k);
                else if (n == "target_features")
                    features = i32(k);
                else if (!customs.push(k))
                    a.oom = true;
            }
        }
    }

    static Str custom_name(Str n)
    {
        Str prefix = ".custom_section.";
        return n.starts_with(prefix) ? n.substr(prefix.size()) : n;
    }

    bool definitions()
    {
        for (u32 s : a.order) {
            Symbol &y = a.syms[s];
            if (y.temp && y.name.empty())
                continue;
            if (y.kind == K_FUNCTION) {
                if (!y.defined)
                    continue;
                if (y.offset != 0)
                    return fatal("function sections must contain one function each");
                index[s] = n_func_imports + u32(functions.size());
                if (!functions.push(s))
                    a.oom = true;
            } else if (is_data(y)) {
                if (!in_symtab(y) || !y.defined)
                    continue;
                if (!y.has_size)
                    return fatal_named("data symbols must have a size set with .size: ", y.name);
                if (a.sections[y.section].kind != S_DATA)
                    return fatal_named("data symbols must live in a data section: ", y.name);
                data_seg[s]  = u32(seg_of[y.section]);
                data_off[s]  = y.offset;
                data_size[s] = y.size;
            } else if (y.kind == K_GLOBAL) {
                if (y.defined) {
                    index[s] = n_global_imports + u32(globals.size());
                    if (!globals.push(s))
                        a.oom = true;
                }
            } else if (y.kind == K_TABLE) {
                if (y.defined) {
                    index[s] = n_table_imports + u32(tables.size());
                    if (!tables.push(s))
                        a.oom = true;
                }
            }
        }
        return true;
    }

    void symbol_table()
    {
        for (u32 s : a.order) {
            const Symbol &y = a.syms[s];
            if (!in_symtab(y))
                continue;
            if (is_data(y) && y.defined && data_seg[s] == NONE_INDEX)
                continue;
            u32 flags = 0;
            if (y.weak)
                flags |= wasm::SYM_WEAK;
            if (y.hidden)
                flags |= wasm::SYM_HIDDEN;
            if (!y.external && y.defined)
                flags |= wasm::SYM_LOCAL;
            if (!y.defined)
                flags |= wasm::SYM_UNDEFINED;
            if (y.no_strip)
                flags |= wasm::SYM_NO_STRIP;
            if (y.has_import)
                flags |= wasm::SYM_EXPLICIT_NAME;
            if (y.has_export)
                flags |= wasm::SYM_EXPORTED;
            u8 kind      = y.kind == K_FUNCTION ? wasm::SYM_FUNCTION
                           : y.kind == K_GLOBAL ? wasm::SYM_GLOBAL
                           : y.kind == K_TABLE  ? wasm::SYM_TABLE
                                                : wasm::SYM_DATA;
            sym_index[s] = u32(symtab.size());
            if (!symtab.push(SymInfo{ kind, flags, s }))
                a.oom = true;
        }
    }

    // Functions whose address is taken get a slot in the table, from 1, in
    // the order of the relocations that take it: code's, then data's.
    void table_elems()
    {
        const Vec<Reloc> *lists[] = { &code_relocs, &data_relocs };
        for (const Vec<Reloc> *list : lists)
            for (const Reloc &r : *list) {
                if (!is_table_index(r.type) || table_index[r.sym])
                    continue;
                table_index[r.sym] = u32(elems.size()) + 1;
                if (!elems.push(index[r.sym]))
                    a.oom = true;
                register_type(r.sym);
            }
    }

    // ------------------------------------------------------------ values

    u64 provisional(const Reloc &r)
    {
        const Symbol &y = a.syms[r.sym];
        switch (r.type) {
        case wasm::R_TABLE_INDEX_SLEB:
        case wasm::R_TABLE_INDEX_SLEB64:
        case wasm::R_TABLE_INDEX_I32:
        case wasm::R_TABLE_INDEX_I64:
            return table_index[r.sym];
        case wasm::R_TYPE_INDEX_LEB:
            return type_index[r.sym];
        case wasm::R_FUNCTION_INDEX_LEB:
        case wasm::R_GLOBAL_INDEX_LEB:
        case wasm::R_GLOBAL_INDEX_I32:
        case wasm::R_TABLE_NUMBER_LEB:
            return index[r.sym];
        default:
            if (!y.defined)
                return 0;
            return segments[data_seg[r.sym]].offset + data_off[r.sym] + u64(r.addend);
        }
    }

    // A section's bytes with every relocated field holding its value.
    void patched(u32 k, const Vec<Reloc> &relocs, Vec<u8> &b)
    {
        b.clear();
        Emit copy{ b };
        copy.bytes(Bytes(a.sections[k].bytes.data(), a.sections[k].bytes.size()));
        a.oom |= copy.oom;
        if (a.oom)
            return;
        for (const Reloc &r : relocs) {
            if (r.section != k)
                continue;
            u64 v  = provisional(r);
            u8 *at = b.data() + r.offset;
            switch (r.type) {
            case wasm::R_FUNCTION_INDEX_LEB:
            case wasm::R_TYPE_INDEX_LEB:
            case wasm::R_GLOBAL_INDEX_LEB:
            case wasm::R_MEMORY_ADDR_LEB:
            case wasm::R_TAG_INDEX_LEB:
            case wasm::R_TABLE_NUMBER_LEB:
                patch_uleb5(at, u32(v));
                break;
            case wasm::R_MEMORY_ADDR_LEB64:
            case wasm::R_TABLE_INDEX_SLEB64:
            case wasm::R_MEMORY_ADDR_SLEB64:
                patch_sleb(at, i64(v), 10);
                break;
            case wasm::R_TABLE_INDEX_I32:
            case wasm::R_MEMORY_ADDR_I32:
            case wasm::R_GLOBAL_INDEX_I32:
                patch_le(at, v, 4);
                break;
            case wasm::R_TABLE_INDEX_I64:
            case wasm::R_MEMORY_ADDR_I64:
                patch_le(at, v, 8);
                break;
            default:
                patch_sleb(at, i32(u32(v)), 5);
                break;
            }
        }
    }

    // ------------------------------------------------------------ writing

    struct Mark {
        usize size_at;
        usize payload;
    };

    Emit e{ out };

    Mark begin(u8 id)
    {
        e.byte(id);
        Mark m{ out.size(), 0 };
        e.uleb5(0);
        m.payload = out.size();
        section_count++;
        return m;
    }

    Mark begin_custom(Str name)
    {
        Mark m = begin(wasm::SEC_CUSTOM);
        e.name(name);
        return m;
    }

    // A linking subsection: an id and a padded size, as a section has.
    Mark begin_sub(u8 id)
    {
        e.byte(id);
        Mark m{ out.size(), 0 };
        e.uleb5(0);
        m.payload = out.size();
        return m;
    }

    void end(const Mark &m)
    {
        if (!e.oom)
            patch_uleb5(out.data() + m.size_at, u32(out.size() - m.payload));
    }

    void types()
    {
        if (sigs.empty())
            return;
        Mark m = begin(wasm::SEC_TYPE);
        e.uleb(u32(sigs.size()));
        Sig none;
        for (u32 k : sigs) {
            const Sig &s = k == NONE_INDEX ? none : a.sigs[k];
            e.byte(wasm::FUNC_TYPE);
            e.uleb(u32(s.params.size()));
            for (u8 t : s.params)
                e.byte(t);
            e.uleb(u32(s.results.size()));
            for (u8 t : s.results)
                e.byte(t);
        }
        end(m);
    }

    void imports_section()
    {
        Mark m = begin(wasm::SEC_IMPORT);
        e.uleb(u32(imports.size()));
        for (const Import &im : imports) {
            e.name(im.module);
            e.name(im.field);
            e.byte(im.kind);
            switch (im.kind) {
            case wasm::EXT_FUNCTION:
                e.uleb(im.sig);
                break;
            case wasm::EXT_GLOBAL:
                e.byte(im.type);
                e.byte(im.mut ? 1 : 0);
                break;
            case wasm::EXT_MEMORY:
                e.uleb(0);
                e.uleb64((memory_size + 65535) / 65536);
                break;
            case wasm::EXT_TABLE:
                // llvm writes the flags, and the initial size its elements
                // need, but no maximum.
                e.byte(im.type);
                e.uleb(im.tflags);
                e.uleb(u32(elems.size()));
                break;
            }
        }
        end(m);
    }

    void functions_section()
    {
        if (functions.empty())
            return;
        Mark m = begin(wasm::SEC_FUNCTION);
        e.uleb(u32(functions.size()));
        for (u32 f : functions)
            e.uleb(type_index[f]);
        end(m);
    }

    void tables_section()
    {
        if (tables.empty())
            return;
        Mark m = begin(wasm::SEC_TABLE);
        e.uleb(u32(tables.size()));
        for (u32 t : tables) {
            const Symbol &y = a.syms[t];
            e.uleb(y.table_elem);
            e.uleb(y.table_flags);
            e.uleb(y.table_min);
            if (y.table_flags & wasm::LIMITS_MAX)
                e.uleb(y.table_max);
        }
        end(m);
    }

    void globals_section()
    {
        if (globals.empty())
            return;
        Mark m = begin(wasm::SEC_GLOBAL);
        e.uleb(u32(globals.size()));
        for (u32 g : globals) {
            const Symbol &y = a.syms[g];
            e.uleb(y.global_type);
            e.byte(y.global_mutable ? 1 : 0);
            switch (y.global_type) {
            case wasm::I32:
                e.byte(wasm::OP_I32_CONST);
                e.sleb(0);
                break;
            case wasm::I64:
                e.byte(wasm::OP_I64_CONST);
                e.sleb(0);
                break;
            case wasm::F32:
                e.byte(wasm::OP_F32_CONST);
                e.u32le(0);
                break;
            case wasm::F64:
                e.byte(wasm::OP_F64_CONST);
                e.u32le(0);
                e.u32le(0);
                break;
            default:
                e.byte(wasm::OP_REF_NULL);
                e.byte(wasm::EXTERNREF);
                break;
            }
            e.byte(wasm::OP_END);
        }
        end(m);
    }

    void exports_section()
    {
        u32 n = 0;
        for (u32 f : functions)
            n += a.syms[f].has_export;
        if (!n)
            return;
        Mark m = begin(wasm::SEC_EXPORT);
        e.uleb(n);
        for (u32 f : functions)
            if (a.syms[f].has_export) {
                e.name(a.syms[f].export_name);
                e.byte(wasm::EXT_FUNCTION);
                e.uleb(index[f]);
            }
        end(m);
    }

    void elem_section()
    {
        if (elems.empty())
            return;
        Mark m = begin(wasm::SEC_ELEM);
        e.uleb(1);
        u32 table = index[a.table];
        u32 flags = table ? 2 : 0;
        e.uleb(flags);
        if (flags)
            e.uleb(table);
        e.byte(wasm::OP_I32_CONST);
        e.sleb(1);
        e.byte(wasm::OP_END);
        if (flags & 3)
            e.byte(0);
        e.uleb(u32(elems.size()));
        for (u32 f : elems)
            e.uleb(f);
        end(m);
    }

    void datacount_section()
    {
        if (segments.empty())
            return;
        Mark m = begin(wasm::SEC_DATACOUNT);
        e.uleb(u32(segments.size()));
        end(m);
    }

    u32 code_section()
    {
        if (functions.empty())
            return 0;
        Mark m  = begin(wasm::SEC_CODE);
        u32 idx = section_count - 1;
        e.uleb(u32(functions.size()));
        Vec<u8> body;
        for (u32 f : functions) {
            u32 k = a.syms[f].section;
            patched(k, code_relocs, body);
            e.uleb(u32(body.size()));
            sec_offset[k] = u32(out.size() - m.payload);
            e.bytes(Bytes(body.data(), body.size()));
        }
        end(m);
        return idx;
    }

    u32 data_section()
    {
        if (segments.empty())
            return 0;
        Mark m  = begin(wasm::SEC_DATA);
        u32 idx = section_count - 1;
        e.uleb(u32(segments.size()));
        Vec<u8> body;
        for (const Segment &g : segments) {
            const Section &s = a.sections[g.section];
            e.uleb(s.passive ? 1 : 0);
            if (!s.passive) {
                e.byte(wasm::OP_I32_CONST);
                e.sleb64(i64(g.offset));
                e.byte(wasm::OP_END);
            }
            patched(g.section, data_relocs, body);
            e.uleb(u32(body.size()));
            sec_offset[g.section] = u32(out.size() - m.payload);
            e.bytes(Bytes(body.data(), body.size()));
        }
        end(m);
        return idx;
    }

    void custom_section(u32 k)
    {
        const Section &s = a.sections[k];
        Mark m           = begin_custom(custom_name(s.name));
        e.bytes(Bytes(s.bytes.data(), s.bytes.size()));
        end(m);
    }

    void linking()
    {
        Mark m = begin_custom("linking");
        e.uleb(wasm::LINKING_VERSION);
        if (!symtab.empty()) {
            Mark sub = begin_sub(wasm::SUB_SYMBOL_TABLE);
            e.uleb(u32(symtab.size()));
            for (const SymInfo &si : symtab) {
                const Symbol &y = a.syms[si.sym];
                e.uleb(si.kind);
                e.uleb(si.flags);
                if (si.kind == wasm::SYM_DATA) {
                    e.name(y.name);
                    if (!(si.flags & wasm::SYM_UNDEFINED)) {
                        e.uleb(data_seg[si.sym]);
                        e.uleb(data_off[si.sym]);
                        e.uleb64(data_size[si.sym]);
                    }
                } else {
                    e.uleb(index[si.sym]);
                    if (!(si.flags & wasm::SYM_UNDEFINED) || (si.flags & wasm::SYM_EXPLICIT_NAME))
                        e.name(y.name);
                }
            }
            end(sub);
        }
        if (!segments.empty()) {
            Mark sub = begin_sub(wasm::SUB_SEGMENT_INFO);
            e.uleb(u32(segments.size()));
            for (const Segment &g : segments) {
                const Section &s = a.sections[g.section];
                e.name(s.name);
                e.uleb(s.align);
                e.uleb(s.flags);
            }
            end(sub);
        }
        end(m);
    }

    void reloc_section(u32 target, Str name, Vec<Reloc> &relocs)
    {
        if (relocs.empty())
            return;
        // In the order of the fields they patch, where the sections landed;
        // a stable sort, as llvm's.
        auto at = [&](const Reloc &r) { return u64(sec_offset[r.section]) + r.offset; };
        for (usize i = 1; i < relocs.size(); i++) {
            Reloc r = relocs[i];
            usize j = i;
            while (j > 0 && at(relocs[j - 1]) > at(r)) {
                relocs[j] = relocs[j - 1];
                j--;
            }
            relocs[j] = r;
        }
        Mark m = begin_custom(name);
        e.uleb(target);
        e.uleb(u32(relocs.size()));
        for (const Reloc &r : relocs) {
            e.byte(r.type);
            e.uleb(u32(at(r)));
            e.uleb(r.type == wasm::R_TYPE_INDEX_LEB ? type_index[r.sym] : sym_index[r.sym]);
            if (wasm::reloc_has_addend(r.type))
                e.sleb64(r.addend);
        }
        end(m);
    }

    bool run()
    {
        // A function's section holds that function alone.
        Vec<i32> owner;
        grow(owner, a.sections.size(), i32(-1));
        for (u32 s : a.order) {
            const Symbol &y = a.syms[s];
            if (y.kind == K_FUNCTION && y.defined) {
                if (owner[y.section] >= 0)
                    return fatal_named("section already has a defining function: ",
                                       a.sections[y.section].name);
                owner[y.section] = i32(s);
            }
        }
        if (!record())
            return false;
        usize n = a.syms.size();
        grow(index, n, NONE_INDEX);
        grow(type_index, n, NONE_INDEX);
        grow(table_index, n, u32(0));
        grow(sym_index, n, NONE_INDEX);
        grow(data_seg, n, NONE_INDEX);
        grow(data_off, n, u32(0));
        grow(data_size, n, u64(0));
        grow(seg_of, a.sections.size(), i32(-1));
        grow(sec_offset, a.sections.size(), u32(0));
        if (a.oom || !imports_of())
            return false;
        segments_of();
        if (!definitions())
            return false;
        symbol_table();
        table_elems();
        if (a.oom)
            return false;

        e.bytes(Bytes(wasm::MAGIC, 4));
        e.u32le(wasm::VERSION);
        types();
        imports_section();
        functions_section();
        tables_section();
        globals_section();
        exports_section();
        elem_section();
        datacount_section();
        u32 code = code_section();
        u32 data = data_section();
        for (u32 k : customs)
            custom_section(k);
        linking();
        reloc_section(code, "reloc.CODE", code_relocs);
        reloc_section(data, "reloc.DATA", data_relocs);
        if (producers >= 0)
            custom_section(u32(producers));
        if (features >= 0)
            custom_section(u32(features));
        return !e.oom && !a.oom;
    }
};

} // namespace

bool write_object(Asm &a, Vec<u8> &out, Diag &diag)
{
    Writer w(a, diag, out);
    if (w.run())
        return true;
    if (!w.failed)
        diag.error("out of memory");
    return false;
}

} // namespace as
