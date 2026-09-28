#include "nm.h"

#include "archive.h"
#include "demangle/demangle.h"
#include "wasm.h"

namespace {

bool is_wasm(Bytes f)
{
    return f.size() >= 4 && f[0] == wasm::MAGIC[0] && f[1] == wasm::MAGIC[1] &&
           f[2] == wasm::MAGIC[2] && f[3] == wasm::MAGIC[3];
}

bool is_bitcode(Bytes f)
{
    return f.size() >= 4 && f[0] == 'B' && f[1] == 'C' && f[2] == 0xc0 && f[3] == 0xde;
}

// Byte order, as std::string compares.
int compare(Str a, Str b)
{
    usize n = a.size() < b.size() ? a.size() : b.size();
    for (usize i = 0; i < n; i++)
        if (a[i] != b[i])
            return u8(a[i]) < u8(b[i]) ? -1 : 1;
    return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
}

int compare(u64 a, u64 b)
{
    return a < b ? -1 : a > b ? 1 : 0;
}

// llvm-nm's order: by address with undefined symbols first (-n), by size,
// or by name; the other keys break ties.
int compare(const NmConfig &c, const ModuleSymbol &a, const ModuleSymbol &b)
{
    int r;
    if (c.numeric_sort) {
        if ((r = compare(u64(!a.undefined), u64(!b.undefined))) || (r = compare(a.addr, b.addr)) ||
            (r = compare(a.name, b.name)))
            return r;
        return compare(a.size, b.size);
    }
    if (c.size_sort) {
        if ((r = compare(a.size, b.size)) || (r = compare(a.name, b.name)))
            return r;
        return compare(a.addr, b.addr);
    }
    if (c.export_symbols)
        return compare(a.name, b.name);
    if ((r = compare(a.name, b.name)) || (r = compare(a.size, b.size)))
        return r;
    return compare(a.addr, b.addr);
}

// A bottom-up merge sort: stable, and no recursion.
bool sort(const NmConfig &c, Vec<ModuleSymbol> &v)
{
    if (c.no_sort || v.size() < 2)
        return true;
    Vec<ModuleSymbol> t;
    if (!t.resize(v.size()))
        return false;
    Vec<ModuleSymbol> *a = &v, *b = &t;
    for (usize w = 1; w < v.size(); w *= 2) {
        for (usize lo = 0; lo < v.size(); lo += 2 * w) {
            usize mid = lo + w < v.size() ? lo + w : v.size();
            usize hi  = lo + 2 * w < v.size() ? lo + 2 * w : v.size();
            usize i = lo, j = mid, o = lo;
            while (i < mid && j < hi) {
                int r      = compare(c, (*a)[j], (*a)[i]);
                bool right = c.reverse_sort ? r > 0 : r < 0;
                (*b)[o++]  = right ? (*a)[j++] : (*a)[i++];
            }
            while (i < mid)
                (*b)[o++] = (*a)[i++];
            while (j < hi)
                (*b)[o++] = (*a)[j++];
        }
        Vec<ModuleSymbol> *x = a;
        a                    = b;
        b                    = x;
    }
    if (a != &v)
        for (usize i = 0; i < v.size(); i++)
            v[i] = t[i];
    return true;
}

bool should_print(const NmConfig &c, const ModuleSymbol &s)
{
    return !((!s.undefined && c.undefined_only) || (s.undefined && c.defined_only) ||
             (!s.global && c.extern_only) || (s.weak && c.no_weak));
}

bool is_defined(const ModuleSymbol &s)
{
    return s.type != 'U' && s.type != 'w' && s.type != 'v';
}

// "%08x", "%08o" or "%08d"; unpadded for -P.
void value(Out &o, u64 v, u32 radix, bool pad)
{
    char d[24];
    usize k = 0;
    do {
        d[k++] = "0123456789abcdef"[v % radix];
        v /= radix;
    } while (v);
    for (usize i = k; pad && i < 8; i++)
        o.put('0');
    while (k)
        o.put(d[--k]);
}

// -A's prefix: "file: ", "archive:member: ", or -P's "archive[member]: ".
void file_name(Out &o, const NmConfig &c, Str archive, Str object)
{
    if (c.format == NmFormat::Posix && !archive.empty()) {
        o.put(archive).put('[').put(object).put("]: ");
        return;
    }
    if (!archive.empty())
        o.put(archive).put(':');
    o.put(object).put(": ");
}

void print(Nm &n, const Vec<ModuleSymbol> &syms, bool print_name, Str archive, Str object)
{
    const NmConfig &c = n.c;
    Out &o            = n.out;
    bool posix        = c.format == NmFormat::Posix;
    if (!c.print_file) {
        if ((c.format == NmFormat::Bsd || posix || c.format == NmFormat::JustSymbols) &&
            n.multiple && print_name) {
            o.put('\n').put(object).put(":\n");
        } else if (c.format == NmFormat::Sysv) {
            o.put("\n\nSymbols from ").put(object).put(":\n\n");
            o.put(
                "Name                  Value   Class        Type         Size     Line  Section\n");
        }
    }
    Out addr, size;
    for (const ModuleSymbol &s : syms) {
        if (!should_print(c, s))
            continue;
        Str name = s.name;
        if (c.demangle) {
            demangle(s.name, n.scratch);
            name = n.scratch.str();
        }
        if (c.print_file)
            file_name(o, c, archive, object);
        if (c.format == NmFormat::JustSymbols) {
            o.put(name).put('\n');
            continue;
        }
        addr.clear();
        size.clear();
        // An undefined symbol has blanks, but for -P's zeros.
        if (is_defined(s) || posix) {
            value(addr, s.addr, c.radix, !posix);
            value(size, s.size, c.radix, !posix);
        } else {
            addr.put("        ");
            size.put("        ");
        }
        if (posix) {
            o.put(name).put(' ').put(s.type).put(' ').put(addr.str()).put(' ').put(size.str());
        } else if (c.format == NmFormat::Sysv) {
            o.left(name, 20).put('|').put(addr.str()).put("|   ").put(s.type);
            o.put("  |                  |").put(size.str()).put("|     |");
        } else {
            if (c.print_address)
                o.put(addr.str()).put(' ');
            if (c.print_size)
                o.put(size.str()).put(' ');
            o.put(s.type).put(' ').put(name);
        }
        o.put('\n');
    }
}

// One module. A member of an archive is labelled, unless -A names it on
// every line.
void module(Nm &n, Str archive, Str object, Str what, Bytes file, Diag &diag)
{
    Out err;
    if (is_bitcode(file)) {
        err.put(what).put(": LLVM bitcode (from -flto), not a wasm module");
        diag.error(err.str());
        return;
    }
    Vec<ModuleSymbol> syms;
    if (!read_symbols(what, file, syms, err)) {
        diag.error(err.str());
        return;
    }
    bool member = !archive.empty();
    if (member && !n.c.print_file && !n.c.export_symbols)
        n.out.put('\n').put(object).put(":\n");
    if (n.c.dynamic) {
        err.put(object).put(": File format has no dynamic symbol table");
        diag.error(err.str());
        return;
    }
    if (n.c.export_symbols) {
        for (const ModuleSymbol &s : syms)
            if (!n.exports.push(s)) {
                diag.error("out of memory");
                return;
            }
        return;
    }
    if (syms.empty() && !n.c.quiet) {
        file_name(diag.text, n.c, archive, object);
        diag.text.put("no symbols\n");
    }
    if (!sort(n.c, syms)) {
        diag.error("out of memory");
        return;
    }
    print(n, syms, !member, archive, object);
}

} // namespace

void nm_settle(NmConfig &c)
{
    if (c.export_symbols)
        c.extern_only = c.defined_only = true;
    if (c.size_sort && !c.print_size)
        c.print_address = false;
    if (c.format == NmFormat::Sysv || c.size_sort)
        c.print_size = true;
}

void nm_file(Nm &n, Str name, Bytes file, Diag &diag)
{
    if (n.c.export_symbols && file.size() >= 2 && file[0] == '#' && file[1] == '!')
        return;
    if (!is_archive(file)) {
        module(n, Str(), name, name, file, diag);
        return;
    }
    Vec<Member> members;
    Vec<ArchiveSymbol> index;
    Out err;
    if (!read_archive(name, file, members, index, err)) {
        diag.error(err.str());
        return;
    }
    if (n.c.print_armap && !index.empty()) {
        n.out.put("Archive map\n");
        for (const ArchiveSymbol &s : index)
            n.out.put(s.name).put(" in ").put(members[s.member].name).put('\n');
        n.out.put('\n');
    }
    for (const Member &m : members) {
        if (!is_wasm(m.data) && !is_bitcode(m.data))
            continue;
        String what;
        if (!what.append(name) || !what.push('(') || !what.append(m.name) || !what.push(')')) {
            diag.error("out of memory");
            return;
        }
        module(n, name, m.name, what.str(), m.data, diag);
    }
}

void nm_exports(Nm &n)
{
    if (!n.c.export_symbols)
        return;
    Vec<ModuleSymbol> kept;
    for (const ModuleSymbol &s : n.exports)
        if (should_print(n.c, s) && !kept.push(s))
            n.out.oom = true;
    if (!sort(n.c, kept))
        n.out.oom = true;
    for (usize i = 0; i < kept.size(); i++)
        if (i == 0 || compare(n.c, kept[i - 1], kept[i]) != 0)
            n.out.put(kept[i].name).put('\n');
}
