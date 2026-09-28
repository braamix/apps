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

bool defined_symbols(Bytes file, Vec<Str> &names)
{
    Vec<Section> sections;
    Out err;
    if (!read_module(Str(), file, sections, err))
        return false;
    const Section *linking = nullptr;
    for (const Section &s : sections)
        if (s.id == SEC_CUSTOM && s.name == "linking")
            linking = &s;
    if (!linking)
        return false;

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
