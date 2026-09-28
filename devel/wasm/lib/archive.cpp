#include "archive.h"

#include "cursor.h"
#include "module.h"
#include "wasm.h"

using namespace wasm;

bool is_archive(Bytes f)
{
    static const char MAGIC_AR[] = "!<arch>\n";
    if (f.size() < 8)
        return false;
    for (u32 i = 0; i < 8; i++)
        if (f[i] != u8(MAGIC_AR[i]))
            return false;
    return true;
}

namespace {

// A decimal field of an ar header, space-padded. NONE if malformed.
u32 decimal(Str s)
{
    while (!s.empty() && s[s.size() - 1] == ' ')
        s = s.substr(0, s.size() - 1);
    if (s.empty() || s.size() > 9)
        return NONE;
    u32 v = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return NONE;
        v = v * 10 + u32(c - '0');
    }
    return v;
}

// A header field other than the size: its leading digits in `base`, as
// libarchive reads it. A blank field is 0.
u64 field(Str s, u32 base)
{
    u64 v = 0;
    for (char c : s) {
        if (c < '0' || c >= char('0' + base))
            break;
        v = v * base + u64(c - '0');
    }
    return v;
}

u64 big_endian(const u8 *p, u32 n)
{
    u64 v = 0;
    for (u32 i = 0; i < n; i++)
        v = v << 8 | p[i];
    return v;
}

bool ar_fail(Out &err, Str name, usize at, Str why)
{
    err.put(name).put(": member at 0x").hex(u32(at)).put(": ").put(why);
    return false;
}

// The GNU symbol table at `at`, its body `body`: a big-endian count of
// `w` bytes, as many header offsets, and as many names. Each offset must be
// one of `hdrs`, the members' header offsets in order.
bool read_index(Str name, Bytes body, usize at, u32 w, Span<const u32> hdrs,
                Vec<ArchiveSymbol> &index, Out &err)
{
    if (body.size() < w)
        return ar_fail(err, name, at, "truncated symbol table");
    u64 count = big_endian(body.data(), w);
    if (count > (body.size() - w) / w)
        return ar_fail(err, name, at, "truncated symbol table");
    const char *names = reinterpret_cast<const char *>(body.data()) + w + count * w;
    Str rest(names, body.size() - w - usize(count) * w);
    if (!index.reserve(index.size() + usize(count))) {
        err.put(name).put(": out of memory");
        return false;
    }
    for (u64 i = 0; i < count; i++) {
        u64 off  = big_endian(body.data() + w + i * w, w);
        usize lo = 0, hi = hdrs.size();
        while (lo < hi) {
            usize mid = (lo + hi) / 2;
            if (hdrs[mid] < off)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo == hdrs.size() || hdrs[lo] != off)
            return ar_fail(err, name, at, "symbol table names an offset that is not a member");
        usize nul = rest.find('\0');
        if (nul == Str::npos)
            return ar_fail(err, name, at, "truncated symbol table");
        index.push(ArchiveSymbol{ rest.substr(0, nul), u32(lo) });
        rest = rest.substr(nul + 1);
    }
    return true;
}

} // namespace

bool read_archive(Str name, Bytes f, Vec<Member> &members, Vec<ArchiveSymbol> &index, Out &err)
{
    const char *text = reinterpret_cast<const char *>(f.data());
    if (f.size() >= 8 && Str(text, 8) == "!<thin>\n") {
        err.put(name).put(": thin archives are not linked here");
        return false;
    }
    if (!is_archive(f)) {
        err.put(name).put(": not an archive");
        return false;
    }
    Str longnames;
    Bytes symtab;
    usize symtab_at = 0;
    u32 symtab_w    = 0;
    Vec<u32> hdrs;
    usize at = 8;
    while (at < f.size()) {
        if (f.size() - at < 60)
            return ar_fail(err, name, at, "truncated header");
        Str hdr(text + at, 60);
        if (hdr[58] != '`' || hdr[59] != '\n')
            return ar_fail(err, name, at, "bad header terminator");
        u32 size = decimal(hdr.substr(48, 10));
        if (size == NONE)
            return ar_fail(err, name, at, "bad member size");
        usize data = at + 60;
        if (size > f.size() - data)
            return ar_fail(err, name, at, "member runs past the end of the archive");

        Str raw = hdr.substr(0, 16);
        while (!raw.empty() && raw[raw.size() - 1] == ' ')
            raw = raw.substr(0, raw.size() - 1);
        Bytes body = f.subspan(data, size);
        Str mname;
        bool skip = false;
        if (raw == "/" || raw == "/SYM64/") {
            symtab    = body;
            symtab_at = at;
            symtab_w  = raw == "/" ? 4 : 8;
            skip      = true;
        } else if (raw == "//") {
            longnames = Str(text + data, size);
            skip      = true;
        } else if (raw.starts_with("#1/")) {
            u32 n = decimal(raw.substr(3));
            if (n == NONE || n > size)
                return ar_fail(err, name, at, "bad BSD name length");
            mname = Str(text + data, n);
            while (!mname.empty() && mname[mname.size() - 1] == '\0')
                mname = mname.substr(0, mname.size() - 1);
            body = body.subspan(n);
        } else if (raw.size() > 1 && raw[0] == '/' && raw[1] >= '0' && raw[1] <= '9') {
            u32 off = decimal(raw.substr(1));
            if (off == NONE || off >= longnames.size())
                return ar_fail(err, name, at, "long name out of range");
            Str rest = longnames.substr(off);
            usize nl = rest.find('\n');
            mname    = nl == Str::npos ? rest : rest.substr(0, nl);
            if (mname.ends_with("/"))
                mname = mname.substr(0, mname.size() - 1);
        } else {
            mname = raw;
            if (mname.ends_with("/"))
                mname = mname.substr(0, mname.size() - 1);
        }
        if (mname.starts_with("__.SYMDEF"))
            skip = true;
        if (!skip) {
            Member m{};
            m.name     = mname;
            m.data     = body;
            m.file_off = u32(data + (size - body.size()));
            m.mtime    = field(hdr.substr(16, 12), 10);
            m.uid      = u32(field(hdr.substr(28, 6), 10));
            m.gid      = u32(field(hdr.substr(34, 6), 10));
            m.mode     = u32(field(hdr.substr(40, 8), 8));
            m.size     = size;
            if (!members.push(m) || !hdrs.push(u32(at))) {
                err.put(name).put(": out of memory");
                return false;
            }
        }
        at = data + size + (size & 1);
    }
    if (symtab_w)
        return read_index(name, symtab, symtab_at, symtab_w, hdrs, index, err);
    return true;
}

namespace {

// A header field: `v` in `base`, left-justified and space-padded to `width`.
void ar_field(Emit &e, u64 v, u32 base, u32 width)
{
    u64 max = 1;
    for (u32 i = 0; i < width; i++)
        max *= base;
    if (v >= max)
        v = max - 1;
    char t[20];
    u32 k = 0;
    do {
        t[k++] = char('0' + v % base);
        v /= base;
    } while (v);
    for (u32 i = 0; i < width; i++)
        e.byte(u8(i < k ? t[k - 1 - i] : ' '));
}

} // namespace

void emit_ar_header(Emit &e, Str name, u64 mtime, u32 uid, u32 gid, u32 mode, u64 size)
{
    for (u32 i = 0; i < 16; i++)
        e.byte(u8(i < name.size() ? name[i] : ' '));
    ar_field(e, mtime, 10, 12);
    ar_field(e, uid, 10, 6);
    ar_field(e, gid, 10, 6);
    ar_field(e, mode, 8, 8);
    ar_field(e, size, 10, 10);
    e.byte('`');
    e.byte('\n');
}

namespace {

// Imported functions and globals, which come first in their index spaces.
void import_counts(const Section *imports, u32 &funcs, u32 &globals)
{
    funcs = globals = 0;
    if (!imports)
        return;
    Cursor c(imports->body);
    u32 n = c.count();
    for (u32 i = 0; i < n && c.ok(); i++) {
        c.name();
        c.name();
        u8 kind = c.byte();
        if (kind == EXT_FUNCTION) {
            c.uleb();
            funcs++;
        } else if (kind == EXT_GLOBAL) {
            c.byte();
            c.byte();
            globals++;
        } else if (kind == EXT_TABLE) {
            c.byte();
            if (c.byte() & 1)
                c.uleb();
            c.uleb();
        } else if (kind == EXT_MEMORY) {
            if (c.byte() & 1)
                c.uleb();
            c.uleb();
        } else {
            c.byte();
            c.uleb();
        }
    }
}

// A linked module's symbols, as llvm reads them. With a name section: named
// functions defined and exported, and named globals defined, in its order.
// Without: the exports but memories.
bool module_symbols(const Section *imports, const Section *exports, const Section *names_sec,
                    Vec<Str> &names)
{
    Vec<u32> exported;
    if (exports) {
        Cursor c(exports->body);
        u32 n = c.count();
        for (u32 i = 0; i < n && c.ok(); i++) {
            Str name = c.name();
            u8 kind  = c.byte();
            u32 idx  = c.uleb();
            if (!c.ok())
                break;
            if (!names_sec && kind != EXT_MEMORY && !names.push(name))
                return false;
            if (kind == EXT_FUNCTION && !exported.push(idx))
                return false;
        }
    }
    if (!names_sec)
        return true;
    u32 funcs, globals;
    import_counts(imports, funcs, globals);
    Cursor c(names_sec->body);
    while (c.ok() && !c.done()) {
        u8 id   = c.byte();
        Bytes b = c.take(c.uleb());
        if (id != 1 && id != 7)
            continue;
        Cursor s(b);
        u32 n = s.count();
        for (u32 i = 0; i < n && s.ok(); i++) {
            u32 idx  = s.uleb();
            Str name = s.name();
            bool in  = false;
            if (id == 7)
                in = idx >= globals;
            else if (idx >= funcs)
                for (u32 e : exported)
                    in = in || e == idx;
            if (s.ok() && in && !names.push(name))
                return false;
        }
    }
    return true;
}

} // namespace

bool defined_symbols(Bytes file, Vec<Str> &names)
{
    Vec<Section> sections;
    Out err;
    if (!read_module(Str(), file, sections, err))
        return false;
    const Section *linking = nullptr, *imports = nullptr, *exports = nullptr, *names_sec = nullptr;
    for (const Section &s : sections) {
        if (s.id == SEC_CUSTOM && s.name == "linking")
            linking = &s;
        if (s.id == SEC_CUSTOM && s.name == "name")
            names_sec = &s;
        if (s.id == SEC_IMPORT)
            imports = &s;
        if (s.id == SEC_EXPORT)
            exports = &s;
    }
    if (!linking)
        return module_symbols(imports, exports, names_sec, names);

    Cursor c(linking->body);
    if (c.uleb() != LINKING_VERSION || !c.ok())
        return false;
    Bytes table;
    while (c.ok() && !c.done()) {
        u8 type = c.byte();
        Bytes b = c.take(c.uleb());
        if (type == SUB_SYMBOL_TABLE)
            table = b;
    }
    if (!c.ok())
        return false;

    Cursor t(table);
    u32 n = t.count();
    for (u32 i = 0; i < n && t.ok(); i++) {
        u8 kind      = t.byte();
        u32 flags    = t.uleb();
        bool defined = !(flags & SYM_UNDEFINED);
        Str name;
        switch (kind) {
        case SYM_FUNCTION:
        case SYM_GLOBAL:
        case SYM_TAG:
        case SYM_TABLE:
            t.uleb();
            if (defined || (flags & SYM_EXPLICIT_NAME))
                name = t.name();
            break;
        case SYM_DATA:
            name = t.name();
            if (defined) {
                t.uleb();
                t.uleb();
                t.uleb();
            }
            break;
        case SYM_SECTION:
            t.uleb();
            break;
        default:
            return false;
        }
        if (t.ok() && defined && kind != SYM_SECTION && !(flags & SYM_LOCAL) && !names.push(name))
            return false;
    }
    return t.ok();
}

ArchiveLayout archive_layout(Bytes f)
{
    ArchiveLayout l{ true, false };
    if (f.size() < 8 + 60)
        return l;
    Str raw(reinterpret_cast<const char *>(f.data()) + 8, 16);
    while (!raw.empty() && raw[raw.size() - 1] == ' ')
        raw = raw.substr(0, raw.size() - 1);
    if (raw == "/" || raw == "/SYM64/") {
        l.symtab = true;
    } else if (raw.starts_with("#1/")) {
        Str name(reinterpret_cast<const char *>(f.data()) + 68, f.size() - 68);
        l.gnu    = false;
        l.symtab = name.starts_with("__.SYMDEF");
    } else if (raw.starts_with("__.SYMDEF")) {
        l.gnu    = false;
        l.symtab = true;
    } else if (!raw.ends_with("/")) {
        l.gnu = false;
    }
    return l;
}

namespace {

void be32(Emit &e, u32 v)
{
    for (u32 i = 4; i-- > 0;)
        e.byte(u8(v >> (8 * i)));
}

} // namespace

void write_archive(Span<const ArchiveEntry> members, bool symtab, Emit &e)
{
    // Long names, "name/\n" each, padded to even.
    Vec<u8> names;
    Vec<u32> name_off;
    auto ok = [&](bool pushed) {
        if (!pushed)
            e.oom = true;
    };
    for (const ArchiveEntry &m : members) {
        ok(name_off.push(names.size()));
        if (m.name.size() <= 15)
            continue;
        for (char c : m.name)
            ok(names.push(u8(c)));
        ok(names.push('/'));
        ok(names.push('\n'));
    }
    if (names.size() & 1)
        ok(names.push('\n'));

    // Symbols, then the headers' offsets.
    Vec<Str> syms;
    Vec<u32> owner;
    if (symtab)
        for (u32 k = 0; k < members.size(); k++) {
            u32 before = syms.size();
            defined_symbols(members[k].data, syms);
            for (u32 j = before; j < syms.size(); j++)
                ok(owner.push(k));
        }
    usize sym_names = 0;
    for (Str n : syms)
        sym_names += n.size() + 1;
    usize table = 4 + 4 * syms.size() + sym_names;
    if (table & 1)
        table++;
    if (syms.empty())
        table = 8;

    Vec<u32> at;
    usize off = 8 + (symtab ? 60 + table : 0) + (names.empty() ? 0 : 60 + names.size());
    for (const ArchiveEntry &m : members) {
        ok(at.push(u32(off)));
        off += 60 + m.data.size() + (m.data.size() & 1);
    }

    e.bytes(Bytes(reinterpret_cast<const u8 *>("!<arch>\n"), 8));
    if (e.oom)
        return;
    if (symtab) {
        emit_ar_header(e, "/", 0, 0, 0, 0, table);
        usize start = e.v.size();
        be32(e, syms.size());
        for (u32 o : owner)
            be32(e, at[o]);
        for (Str n : syms) {
            e.bytes(Bytes(reinterpret_cast<const u8 *>(n.data()), n.size()));
            e.byte(0);
        }
        while (e.v.size() - start < table)
            e.byte(0);
    }
    if (!names.empty()) {
        usize start = e.v.size();
        emit_ar_header(e, "//", 0, 0, 0, 0, names.size());
        if (!e.oom)
            for (usize i = 16; i < 48; i++)
                e.v[start + i] = ' ';
        e.bytes(Bytes(names.data(), names.size()));
    }
    for (u32 k = 0; k < members.size(); k++) {
        const ArchiveEntry &m = members[k];
        char field[17];
        usize n = 0;
        if (m.name.size() <= 15) {
            for (char c : m.name)
                field[n++] = c;
            field[n++] = '/';
        } else {
            Out o;
            o.put('/').num(name_off[k]);
            for (char c : o.s.str())
                field[n++] = c;
        }
        emit_ar_header(e, Str(field, n), 0, 0, 0, 0644, m.data.size());
        e.bytes(m.data);
        if (m.data.size() & 1)
            e.byte('\n');
    }
}
