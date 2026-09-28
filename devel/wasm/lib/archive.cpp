#include "archive.h"

#include "module.h"

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

bool ar_fail(Out &err, Str name, usize at, Str why)
{
    err.put(name).put(": member at 0x").hex(u32(at)).put(": ").put(why);
    return false;
}

} // namespace

bool read_archive(Str name, Bytes f, Vec<Member> &members, Out &err)
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
            skip = true;
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
        if (!skip && !members.push(Member{ mname, body, u32(data + (size - body.size())) })) {
            err.put(name).put(": out of memory");
            return false;
        }
        at = data + size + (size & 1);
    }
    return true;
}
