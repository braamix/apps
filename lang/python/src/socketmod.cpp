// `_socket`: the floor under socket.py, with no network under it.
//
// Braam has no sockets and will not grow any, so this module exists to be
// imported rather than used: pdb imports socket, doctest imports pdb, and
// xml.sax.saxutils reaches it through urllib.request. What it answers is the
// truth -- the constants and the exception types are real, gethostname is
// "localhost", and a socket cannot be made, so the constructor raises
// OSError(EAFNOSUPPORT) the way a kernel without that address family does.
//
// The four byte-order calls and the four address-text calls are arithmetic
// and are therefore whole: nothing about inet_ntoa needs a network.
#include "bigint.h"
#include "builtin.h"
#include "exc.h"
#include "gc.h"
#include "intern.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "method.h"
#include "module.h"
#include "obj.h"
#include "ops.h"
#include "type.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

// The two exception classes this module owns. error and timeout are OSError
// and TimeoutError themselves, as they have been since 3.10, so only these
// two are made here.
struct Home {
    Value herror;
    Value gaierror;
};

Home *home;

void home_mark()
{
    if (home) {
        gc_mark(home->herror);
        gc_mark(home->gaierror);
    }
}

// `cls(code, text)`, which is how getaddrinfo and its kin report.
R raise_pair(Value cls, i64 code, Str text)
{
    Root c{ cls };
    Root m{ str_new(text) };
    TupleObj *t = m.v.is_nil() ? nullptr : tuple_new(2);
    if (!t)
        return err_pending() ? R::Err : oom();
    t->items()[0] = Value::of_int(i32(code));
    t->items()[1] = m.v;
    Root rt{ obj_value(t) };
    Value e = exc_construct(c.v, rt.v);
    return e.is_nil() ? R::Err : err_set_value(e);
}

// EAI_NONAME: there is no resolver, so every name is one nobody knows.
R no_name()
{
    return raise_pair(home->gaierror, -2, "Name or service not known");
}

// ------------------------------------------------------------- the type

// Nothing is ever laid out here: __init__ raises before an instance can be
// filled in. `plain` so that socket.py's subclass is an ordinary object with
// the __slots__ it asks for.
constexpr Type socket_type{ .name = "_socket.socket", .plain = true };

// What a kernel with no such address family says. socket.py's socket calls
// this one explicitly from its own __init__, which is where a program that
// asks for a socket ends up.
R sock_init(const CallArgs &a, Value &out)
{
    (void)out;
    if (!meth_args(a, "__init__", 0, 4))
        return R::Err;
    return err_errno(97); // EAFNOSUPPORT
}

constexpr Method SOCKET_METHODS[] = { { "__init__", sock_init } };

// The same refusal one step earlier, for `_socket.socket(...)` itself and for
// the subclass socket.py makes: __new__ is where the making stops.
R b_socket_new(const CallArgs &a, Value &out)
{
    (void)out;
    return args_only(a, "socket", 0, 4) ? err_errno(97) : R::Err;
}

// ------------------------------------------------------------- the names

R s_gethostname(const CallArgs &a, Value &out)
{
    if (!args_only(a, "gethostname", 0, 0))
        return R::Err;
    out = str_new("localhost");
    return out.is_nil() ? R::Err : R::Ok;
}

R s_gethostbyname(const CallArgs &a, Value &out)
{
    (void)out;
    return args_only(a, "gethostbyname", 1, 1) ? no_name() : R::Err;
}

R s_gethostbyname_ex(const CallArgs &a, Value &out)
{
    (void)out;
    return args_only(a, "gethostbyname_ex", 1, 1) ? no_name() : R::Err;
}

R s_gethostbyaddr(const CallArgs &a, Value &out)
{
    (void)out;
    return args_only(a, "gethostbyaddr", 1, 1) ? no_name() : R::Err;
}

R s_getfqdn(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getfqdn", 0, 1))
        return R::Err;
    out = str_new("localhost");
    return out.is_nil() ? R::Err : R::Ok;
}

R s_getaddrinfo(const CallArgs &a, Value &out)
{
    (void)out;
    return args_only(a, "getaddrinfo", 2, 6) ? no_name() : R::Err;
}

R s_getnameinfo(const CallArgs &a, Value &out)
{
    (void)out;
    return args_only(a, "getnameinfo", 2, 2) ? no_name() : R::Err;
}

// /etc/services is not here either, and neither is /etc/protocols beyond the
// four numbers everything knows.
R s_getservbyname(const CallArgs &a, Value &out)
{
    (void)out;
    if (!args_only(a, "getservbyname", 1, 2))
        return R::Err;
    return err_set("OSError", "service/proto not found");
}

R s_getservbyport(const CallArgs &a, Value &out)
{
    (void)out;
    if (!args_only(a, "getservbyport", 1, 2))
        return R::Err;
    return err_set("OSError", "port/proto not found");
}

R s_getprotobyname(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getprotobyname", 1, 1) || !is_str(a.args[0]))
        return err_pending() ? R::Err : err_set("TypeError", "getprotobyname() argument 1 "
                                                             "must be str");
    constexpr struct {
        Str name;
        i32 num;
    } PROTOS[] = {
        { "ip", 0 },  { "icmp", 1 }, { "igmp", 2 },  { "tcp", 6 },
        { "udp", 17 }, { "ipv6", 41 }, { "icmpv6", 58 }, { "raw", 255 },
    };
    Str want = str_of(a.args[0])->str();
    for (const auto &p : PROTOS)
        if (p.name == want) {
            out = Value::of_int(p.num);
            return R::Ok;
        }
    return err_set("OSError", "protocol not found");
}

R s_dup(const CallArgs &a, Value &out)
{
    (void)out;
    if (!args_only(a, "dup", 1, 1))
        return R::Err;
    return err_errno(88); // ENOTSOCK: nothing open here is one
}

// ------------------------------------------------- the default timeout

// A module-level double, as CPython's defaulttimeout is: negative for None.
f64 default_timeout = -1;

R s_getdefaulttimeout(const CallArgs &a, Value &out)
{
    if (!args_only(a, "getdefaulttimeout", 0, 0))
        return R::Err;
    out = default_timeout < 0 ? value_none() : float_new(default_timeout);
    return out.is_nil() ? R::Err : R::Ok;
}

R s_setdefaulttimeout(const CallArgs &a, Value &out)
{
    if (!args_only(a, "setdefaulttimeout", 1, 1))
        return R::Err;
    if (is_none(a.args[0])) {
        default_timeout = -1;
    } else {
        f64 t = 0;
        if (!as_number(a.args[0], t))
            return err_set("TypeError", "a float is required");
        if (t < 0)
            return err_set("ValueError", "Timeout value out of range");
        default_timeout = t;
    }
    out = value_none();
    return R::Ok;
}

// ------------------------------------------------------- the byte orders

// wasm32 is little-endian, so the network order is the swapped one. CPython
// refuses a value that does not fit rather than truncating it.
bool order_arg(const CallArgs &a, Str who, u32 bits, i64 &v)
{
    if (!args_only(a, who, 1, 1) || !as_index(a.args[0], v))
        return false;
    if (v < 0)
        return err_set("OverflowError", "can't convert negative number to unsigned long") == R::Ok;
    if (bits == 16 && v > 0xffff)
        return err_set("OverflowError", "value too large to convert to unsigned short") == R::Ok;
    if (bits == 32 && v > 0xffffffff)
        return err_set("OverflowError", "int larger than 32 bits") == R::Ok;
    return true;
}

Value u32_value(u64 v)
{
    return v <= 0x7fffffffu ? Value::of_int(i32(v)) : int_from_i64(i64(v));
}

R swap16(const CallArgs &a, Value &out)
{
    i64 v = 0;
    if (!order_arg(a, "ntohs", 16, v))
        return R::Err;
    out = Value::of_int(i32(((v & 0xff) << 8) | ((v >> 8) & 0xff)));
    return R::Ok;
}

R swap32(const CallArgs &a, Value &out)
{
    i64 v = 0;
    if (!order_arg(a, "ntohl", 32, v))
        return R::Err;
    u32 n = u32(v);
    out   = u32_value(u64(n >> 24 | (n >> 8 & 0xff00) | (n << 8 & 0xff0000) | n << 24));
    return R::Ok;
}

// ------------------------------------------------------ the address text

// Four decimal octets, as inet_aton takes them: it is laxer than inet_pton
// and accepts one, two, three or four parts, the last soaking up the rest.
bool aton(Str s, u32 &out)
{
    u32 part[4] = { 0, 0, 0, 0 };
    usize n     = 0;
    usize i     = 0;
    while (i < s.size()) {
        if (n == 4 || i >= s.size() || s[i] < '0' || s[i] > '9')
            return false;
        u64 v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + u64(s[i++] - '0');
            if (v > 0xffffffffu)
                return false;
        }
        part[n++] = u32(v);
        if (i == s.size())
            break;
        if (s[i] != '.')
            return false;
        i++;
        if (i == s.size())
            return false;
    }
    if (!n)
        return false;
    // The last part is the whole of what the earlier ones left.
    for (usize k = 0; k + 1 < n; k++)
        if (part[k] > 0xff)
            return false;
    u64 rest = 0xffffffffu >> (8 * (n - 1));
    if (u64(part[n - 1]) > rest)
        return false;
    u32 v = 0;
    for (usize k = 0; k + 1 < n; k++)
        v = (v << 8) | part[k];
    out = u32((u64(v) << (32 - 8 * (n - 1))) | u64(part[n - 1]));
    return true;
}

// Strict dotted quad, which is what inet_pton(AF_INET) takes.
bool pton4(Str s, u32 &out)
{
    u32 v   = 0;
    usize i = 0;
    for (u32 part = 0; part < 4; part++) {
        if (part && (i == s.size() || s[i++] != '.'))
            return false;
        if (i == s.size() || s[i] < '0' || s[i] > '9')
            return false;
        u32 n     = 0;
        usize had = i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            n = n * 10 + u32(s[i++] - '0');
            if (n > 255 || i - had > 3)
                return false;
        }
        // A leading zero is a refusal, not an octal digit.
        if (s[had] == '0' && i - had > 1)
            return false;
        v = (v << 8) | n;
    }
    out = v;
    return i == s.size();
}

int hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// The sixteen octets of an IPv6 address, `::` and a trailing dotted quad
// included.
bool pton6(Str s, u8 out[16])
{
    u8 head[16], tail[16];
    usize nh = 0, nt = 0;
    bool gap = false;
    usize i  = 0;
    if (s.size() >= 2 && s[0] == ':' && s[1] == ':') {
        gap = true;
        i   = 2;
    } else if (!s.empty() && s[0] == ':') {
        return false;
    }
    while (i < s.size()) {
        u8 *at    = gap ? tail : head;
        usize &n  = gap ? nt : nh;
        // A dotted quad may only close the address.
        usize dot = i;
        while (dot < s.size() && s[dot] != ':')
            dot++;
        bool quad = false;
        for (usize k = i; k < dot; k++)
            if (s[k] == '.')
                quad = true;
        if (quad) {
            u32 v = 0;
            if (dot != s.size() || !pton4(s.substr(i, dot - i), v) || n + 4 > 16)
                return false;
            for (u32 k = 0; k < 4; k++)
                at[n++] = u8(v >> (24 - 8 * k));
            i = dot;
            break;
        }
        u32 v     = 0;
        usize had = i;
        while (i < s.size() && hex_digit(s[i]) >= 0) {
            v = v * 16 + u32(hex_digit(s[i++]));
            if (i - had > 4)
                return false;
        }
        if (i == had || n + 2 > 16)
            return false;
        at[n++] = u8(v >> 8);
        at[n++] = u8(v);
        if (i == s.size())
            break;
        if (s[i] != ':')
            return false;
        i++;
        if (i < s.size() && s[i] == ':') {
            if (gap)
                return false;
            gap = true;
            i++;
            if (i == s.size())
                break;
        } else if (i == s.size()) {
            return false;
        }
    }
    if (gap ? nh + nt >= 16 : nh != 16)
        return false;
    for (usize k = 0; k < nh; k++)
        out[k] = head[k];
    for (usize k = nh; k < 16 - nt; k++)
        out[k] = 0;
    for (usize k = 0; k < nt; k++)
        out[16 - nt + k] = tail[k];
    return true;
}

// The canonical text of sixteen octets: RFC 5952's, which is what inet_ntop
// writes -- lower case, the longest run of zero groups folded to `::`.
bool ntop6(const u8 *p, Buf<48> &b)
{
    u32 g[8];
    for (u32 i = 0; i < 8; i++)
        g[i] = u32(p[2 * i]) << 8 | p[2 * i + 1];
    i32 best = -1, bestlen = 1;
    for (i32 i = 0; i < 8; i++) {
        if (g[i])
            continue;
        i32 j = i;
        while (j < 8 && !g[j])
            j++;
        if (j - i > bestlen) {
            best    = i;
            bestlen = j - i;
        }
        i = j - 1;
    }
    for (i32 i = 0; i < 8; i++) {
        if (i == best) {
            b.put("::");
            i += bestlen - 1;
            continue;
        }
        if (i && i != best + bestlen)
            b.put(':');
        char t[8];
        usize n = 0;
        u32 v   = g[i];
        do {
            t[n++] = "0123456789abcdef"[v & 15];
            v >>= 4;
        } while (v);
        while (n)
            b.put(t[--n]);
    }
    return true;
}

// The four octets of an AF_INET address, big-endian, as bytes.
Value packed4(u32 v)
{
    char p[4] = { char(v >> 24), char(v >> 16), char(v >> 8), char(v) };
    return bytes_new(Str(p, 4));
}

R s_inet_aton(const CallArgs &a, Value &out)
{
    if (!args_only(a, "inet_aton", 1, 1) || !is_str(a.args[0]))
        return err_pending() ? R::Err : err_set("TypeError", "inet_aton() argument 1 must be str");
    u32 v = 0;
    if (!aton(str_of(a.args[0])->str(), v))
        return err_set("OSError", "illegal IP address string passed to inet_aton");
    out = packed4(v);
    return out.is_nil() ? R::Err : R::Ok;
}

// The dotted quad of four octets.
void ntop4(const char *p, Buf<24> &b)
{
    char t[12];
    for (u32 i = 0; i < 4; i++) {
        if (i)
            b.put('.');
        b.put(int_text(t, sizeof t, i64(u8(p[i]))));
    }
}

R s_inet_ntoa(const CallArgs &a, Value &out)
{
    Str p;
    if (!args_only(a, "inet_ntoa", 1, 1) || !buffer_like(a.args[0], p))
        return err_pending() ? R::Err : err_set("TypeError", "a bytes-like object is required");
    if (p.size() != 4)
        return err_set("OSError", "packed IP wrong length for inet_ntoa");
    Buf<24> b;
    ntop4(p.data(), b);
    out = str_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

R s_inet_pton(const CallArgs &a, Value &out)
{
    i64 af = 0;
    if (!args_only(a, "inet_pton", 2, 2) || !as_int_arg(a.args[0], af))
        return err_pending() ? R::Err : err_set("TypeError", "an integer is required");
    if (!is_str(a.args[1]))
        return err_set("TypeError", "inet_pton() argument 2 must be str");
    Str s = str_of(a.args[1])->str();
    if (af == 2) { // AF_INET
        u32 v = 0;
        if (!pton4(s, v))
            return err_set2("OSError", "illegal IP address string passed to inet_pton", s);
        out = packed4(v);
    } else if (af == 10) { // AF_INET6
        u8 p[16];
        if (!pton6(s, p))
            return err_set2("OSError", "illegal IP address string passed to inet_pton", s);
        out = bytes_new(Str(reinterpret_cast<const char *>(p), 16));
    } else {
        return err_errno(97); // EAFNOSUPPORT
    }
    return out.is_nil() ? R::Err : R::Ok;
}

R s_inet_ntop(const CallArgs &a, Value &out)
{
    i64 af = 0;
    Str p;
    if (!args_only(a, "inet_ntop", 2, 2) || !as_int_arg(a.args[0], af))
        return err_pending() ? R::Err : err_set("TypeError", "an integer is required");
    if (!buffer_like(a.args[1], p))
        return err_set("TypeError", "a bytes-like object is required");
    if (af != 2 && af != 10)
        return err_errno(97); // EAFNOSUPPORT
    if (p.size() != (af == 2 ? 4u : 16u))
        return err_set("ValueError", "invalid length of packed IP address string");
    Buf<48> b;
    if (af == 2) {
        Buf<24> q;
        ntop4(p.data(), q);
        b.put(q.str());
    } else {
        ntop6(reinterpret_cast<const u8 *>(p.data()), b);
    }
    out = str_new(b.str());
    return out.is_nil() ? R::Err : R::Ok;
}

constexpr ModDef DEFS[] = {
    { "gethostname", s_gethostname },
    { "gethostbyname", s_gethostbyname },
    { "gethostbyname_ex", s_gethostbyname_ex },
    { "gethostbyaddr", s_gethostbyaddr },
    { "getfqdn", s_getfqdn },
    { "getaddrinfo", s_getaddrinfo },
    { "getnameinfo", s_getnameinfo },
    { "getservbyname", s_getservbyname },
    { "getservbyport", s_getservbyport },
    { "getprotobyname", s_getprotobyname },
    { "dup", s_dup },
    { "getdefaulttimeout", s_getdefaulttimeout },
    { "setdefaulttimeout", s_setdefaulttimeout },
    { "ntohs", swap16 },
    { "htons", swap16 },
    { "ntohl", swap32 },
    { "htonl", swap32 },
    { "inet_aton", s_inet_aton },
    { "inet_ntoa", s_inet_ntoa },
    { "inet_pton", s_inet_pton },
    { "inet_ntop", s_inet_ntop },
};

// Linux's numbers, because a program that spells a constant out expects
// those, and because the IntEnums socket.py builds are named by prefix: AF_,
// SOCK_, MSG_ and AI_ each become one.
struct Constant {
    Str name;
    i64 v;
};

constexpr Constant CONSTANTS[] = {
    { "AF_UNSPEC", 0 },
    { "AF_UNIX", 1 },
    { "AF_INET", 2 },
    { "AF_INET6", 10 },
    { "AF_PACKET", 17 },

    { "SOCK_STREAM", 1 },
    { "SOCK_DGRAM", 2 },
    { "SOCK_RAW", 3 },
    { "SOCK_RDM", 4 },
    { "SOCK_SEQPACKET", 5 },
    { "SOCK_CLOEXEC", 0x80000 },
    { "SOCK_NONBLOCK", 0x800 },

    { "SOL_SOCKET", 1 },
    { "SOL_IP", 0 },
    { "SOL_TCP", 6 },
    { "SOL_UDP", 17 },

    { "SO_DEBUG", 1 },
    { "SO_REUSEADDR", 2 },
    { "SO_TYPE", 3 },
    { "SO_ERROR", 4 },
    { "SO_DONTROUTE", 5 },
    { "SO_BROADCAST", 6 },
    { "SO_SNDBUF", 7 },
    { "SO_RCVBUF", 8 },
    { "SO_KEEPALIVE", 9 },
    { "SO_OOBINLINE", 10 },
    { "SO_LINGER", 13 },
    { "SO_REUSEPORT", 15 },
    { "SO_PASSCRED", 16 },
    { "SO_PEERCRED", 17 },
    { "SO_RCVLOWAT", 18 },
    { "SO_SNDLOWAT", 19 },
    { "SO_RCVTIMEO", 20 },
    { "SO_SNDTIMEO", 21 },
    { "SO_BINDTODEVICE", 25 },
    { "SO_ACCEPTCONN", 30 },
    { "SO_PROTOCOL", 38 },
    { "SO_DOMAIN", 39 },
    { "SOMAXCONN", 4096 },
    { "SCM_RIGHTS", 1 },
    { "SCM_CREDENTIALS", 2 },

    { "MSG_OOB", 0x01 },
    { "MSG_PEEK", 0x02 },
    { "MSG_DONTROUTE", 0x04 },
    { "MSG_CTRUNC", 0x08 },
    { "MSG_TRUNC", 0x20 },
    { "MSG_DONTWAIT", 0x40 },
    { "MSG_EOR", 0x80 },
    { "MSG_WAITALL", 0x100 },
    { "MSG_CONFIRM", 0x800 },
    { "MSG_ERRQUEUE", 0x2000 },
    { "MSG_NOSIGNAL", 0x4000 },
    { "MSG_MORE", 0x8000 },
    { "MSG_FASTOPEN", 0x20000000 },
    { "MSG_CMSG_CLOEXEC", 0x40000000 },

    { "IPPROTO_IP", 0 },
    { "IPPROTO_ICMP", 1 },
    { "IPPROTO_IGMP", 2 },
    { "IPPROTO_IPIP", 4 },
    { "IPPROTO_TCP", 6 },
    { "IPPROTO_EGP", 8 },
    { "IPPROTO_PUP", 12 },
    { "IPPROTO_UDP", 17 },
    { "IPPROTO_IDP", 22 },
    { "IPPROTO_TP", 29 },
    { "IPPROTO_DCCP", 33 },
    { "IPPROTO_IPV6", 41 },
    { "IPPROTO_ROUTING", 43 },
    { "IPPROTO_FRAGMENT", 44 },
    { "IPPROTO_RSVP", 46 },
    { "IPPROTO_GRE", 47 },
    { "IPPROTO_ESP", 50 },
    { "IPPROTO_AH", 51 },
    { "IPPROTO_ICMPV6", 58 },
    { "IPPROTO_NONE", 59 },
    { "IPPROTO_DSTOPTS", 60 },
    { "IPPROTO_MTP", 92 },
    { "IPPROTO_ENCAP", 98 },
    { "IPPROTO_PIM", 103 },
    { "IPPROTO_COMP", 108 },
    { "IPPROTO_SCTP", 132 },
    { "IPPROTO_UDPLITE", 136 },
    { "IPPROTO_MPLS", 137 },
    { "IPPROTO_RAW", 255 },

    { "IP_TOS", 1 },
    { "IP_TTL", 2 },
    { "IP_HDRINCL", 3 },
    { "IP_OPTIONS", 4 },
    { "IP_RECVOPTS", 6 },
    { "IP_RETOPTS", 7 },
    { "IP_PKTINFO", 8 },
    { "IP_RECVTOS", 13 },
    { "IP_TRANSPARENT", 19 },
    { "IP_MULTICAST_IF", 32 },
    { "IP_MULTICAST_TTL", 33 },
    { "IP_MULTICAST_LOOP", 34 },
    { "IP_ADD_MEMBERSHIP", 35 },
    { "IP_DROP_MEMBERSHIP", 36 },

    { "IPV6_CHECKSUM", 7 },
    { "IPV6_NEXTHOP", 9 },
    { "IPV6_UNICAST_HOPS", 16 },
    { "IPV6_MULTICAST_IF", 17 },
    { "IPV6_MULTICAST_HOPS", 18 },
    { "IPV6_MULTICAST_LOOP", 19 },
    { "IPV6_JOIN_GROUP", 20 },
    { "IPV6_LEAVE_GROUP", 21 },
    { "IPV6_V6ONLY", 26 },
    { "IPV6_PKTINFO", 50 },
    { "IPV6_HOPLIMIT", 52 },
    { "IPV6_HOPOPTS", 54 },
    { "IPV6_RTHDR", 57 },
    { "IPV6_DSTOPTS", 59 },
    { "IPV6_RECVPKTINFO", 49 },
    { "IPV6_RECVHOPLIMIT", 51 },
    { "IPV6_RECVHOPOPTS", 53 },
    { "IPV6_RECVRTHDR", 56 },
    { "IPV6_RECVDSTOPTS", 58 },
    { "IPV6_RECVTCLASS", 66 },
    { "IPV6_TCLASS", 67 },

    { "TCP_NODELAY", 1 },
    { "TCP_MAXSEG", 2 },
    { "TCP_CORK", 3 },
    { "TCP_KEEPIDLE", 4 },
    { "TCP_KEEPINTVL", 5 },
    { "TCP_KEEPCNT", 6 },
    { "TCP_SYNCNT", 7 },
    { "TCP_LINGER2", 8 },
    { "TCP_DEFER_ACCEPT", 9 },
    { "TCP_WINDOW_CLAMP", 10 },
    { "TCP_INFO", 11 },
    { "TCP_QUICKACK", 12 },
    { "TCP_CONGESTION", 13 },
    { "TCP_USER_TIMEOUT", 18 },
    { "TCP_FASTOPEN", 23 },
    { "TCP_NOTSENT_LOWAT", 25 },

    { "AI_PASSIVE", 1 },
    { "AI_CANONNAME", 2 },
    { "AI_NUMERICHOST", 4 },
    { "AI_V4MAPPED", 8 },
    { "AI_ALL", 16 },
    { "AI_ADDRCONFIG", 32 },
    { "AI_NUMERICSERV", 1024 },

    { "NI_NUMERICHOST", 1 },
    { "NI_NUMERICSERV", 2 },
    { "NI_NOFQDN", 4 },
    { "NI_NAMEREQD", 8 },
    { "NI_DGRAM", 16 },
    { "NI_MAXHOST", 1025 },
    { "NI_MAXSERV", 32 },

    { "EAI_BADFLAGS", -1 },
    { "EAI_NONAME", -2 },
    { "EAI_AGAIN", -3 },
    { "EAI_FAIL", -4 },
    { "EAI_NODATA", -5 },
    { "EAI_FAMILY", -6 },
    { "EAI_SOCKTYPE", -7 },
    { "EAI_SERVICE", -8 },
    { "EAI_ADDRFAMILY", -9 },
    { "EAI_MEMORY", -10 },
    { "EAI_SYSTEM", -11 },
    { "EAI_OVERFLOW", -12 },

    { "SHUT_RD", 0 },
    { "SHUT_WR", 1 },
    { "SHUT_RDWR", 2 },

    { "INADDR_ANY", 0 },
    { "INADDR_LOOPBACK", 0x7f000001 },
    { "INADDR_UNSPEC_GROUP", 0xe0000000 },
    { "INADDR_ALLHOSTS_GROUP", 0xe0000001 },
    { "INADDR_MAX_LOCAL_GROUP", 0xe00000ff },
    { "INADDR_BROADCAST", 0xffffffff },
    { "INADDR_NONE", 0xffffffff },
};

} // namespace

bool socket_install(DictObj *into)
{
    Root rd{ obj_value(into) };
    if (!home) {
        home = heap_new<Home>();
        if (!home)
            return false;
        gc_root_hook(home_mark);
    }
    if (home->herror.is_nil()) {
        home->herror   = mod_exc_class("socket", "herror", "OSError");
        home->gaierror = mod_exc_class("socket", "gaierror", "OSError");
        if (home->herror.is_nil() || home->gaierror.is_nil())
            return false;
    }
    DictObj *d = static_cast<DictObj *>(rd.v.obj());
    if (!method_install(&socket_type, SOCKET_METHODS) || !mod_type(d, &socket_type, b_socket_new) ||
        !mod_defs(d, DEFS))
        return false;
    for (const Constant &c : CONSTANTS)
        if (!mod_int(d, c.name, c.v))
            return false;
    // socket.error is OSError and socket.timeout is TimeoutError, as both
    // have been since 3.10; SocketType is the type under its older name.
    Value os_err, timeout, sock;
    StrObj *n1 = str_intern("OSError");
    StrObj *n2 = str_intern("TimeoutError");
    StrObj *n3 = str_intern("socket");
    if (!n1 || !n2 || !n3 || dict_get(builtins_dict(), obj_value(n1), os_err) != R::Ok ||
        dict_get(builtins_dict(), obj_value(n2), timeout) != R::Ok ||
        dict_get(d, obj_value(n3), sock) != R::Ok)
        return false;
    return mod_put(d, "error", os_err) && mod_put(d, "timeout", timeout) &&
           mod_put(d, "SocketType", sock) && mod_put(d, "herror", home->herror) &&
           mod_put(d, "gaierror", home->gaierror) && mod_put(d, "has_ipv6", value_bool(true));
}
