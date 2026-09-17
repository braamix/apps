// The format-spec mini-language: parsing a spec, and rendering str, int,
// float and bool by one.
//
// The other two grammars -- `%` and str.format's replacement fields -- are in
// formatgr.cpp; both end here.
#include "format.h"

#include "bigint.h"
#include "kernel/fmt.h"
#include "kernel/text.h"
#include "math/ftoa.h"
#include "math/math.h"
#include "ops.h"
#include "type.h"
#include "ustr.h"

namespace {

R oom()
{
    return err_set("MemoryError", "out of memory");
}

bool is_dig(char c)
{
    return c >= '0' && c <= '9';
}

// The one character at `at`, and how many octets it took.
u32 rune_at(Str s, usize at, usize &used)
{
    u32 cp = 0;
    used   = cp_decode(s, at, cp);
    if (!used)
        used = 1;
    return cp;
}

bool is_align(char c)
{
    return c == '<' || c == '>' || c == '^' || c == '=';
}

// One codepoint as \xNN, \uNNNN or \UNNNNNNNN, which is what `!a` wants.
R put_escape(String &out, u32 cp)
{
    const char *HEX = "0123456789abcdef";
    usize digits    = cp > 0xffff ? 8 : cp > 0xff ? 4 : 2;
    char pre        = cp > 0xffff ? 'U' : cp > 0xff ? 'u' : 'x';
    if (!out.push('\\') || !out.push(pre))
        return oom();
    for (usize i = digits; i-- > 0;)
        if (!out.push(HEX[(cp >> (i * 4)) & 0xf]))
            return oom();
    return R::Ok;
}

R bad_spec(Str spec, Str who)
{
    Buf<128> b;
    b.put("Invalid format specifier '").put(spec).put("' for object of type '").put(who).put("'");
    return err_set("ValueError", b.str());
}

} // namespace

bool spec_parse(Str s, Spec &out, Str who)
{
    Spec sp;
    usize i = 0;

    // A fill character is recognised only by the alignment behind it, so the
    // two-character case has to be tried before the one-character one.
    bool filled = false;
    if (s.size() >= 2) {
        usize used = 0;
        u32 cp     = rune_at(s, 0, used);
        if (used < s.size() && is_align(s[used])) {
            filled   = true;
            sp.fill  = cp;
            sp.align = s[used];
            i        = used + 1;
        }
    }
    if (!sp.align && i < s.size() && is_align(s[i]))
        sp.align = s[i++];

    if (i < s.size() && (s[i] == '+' || s[i] == '-' || s[i] == ' '))
        sp.sign = s[i++];
    if (i < s.size() && s[i] == 'z')
        i++; // 3.11's negative-zero coercion; accepted and ignored
    if (i < s.size() && s[i] == '#')
        sp.alt = true, i++;
    if (i < s.size() && s[i] == '0') {
        sp.zero = true;
        i++;
        if (!filled)
            sp.fill = '0';
        if (!sp.align) {
            sp.fill    = '0';
            sp.align   = '=';
            sp.implied = true;
        }
    }

    i64 w = 0;
    if (i < s.size() && is_dig(s[i])) {
        while (i < s.size() && is_dig(s[i])) {
            w = w * 10 + (s[i++] - '0');
            if (w > 0x7fffff)
                return err_set("ValueError", "too many decimal digits in format string"), false;
        }
        sp.width = i32(w);
    }

    if (i < s.size() && (s[i] == ',' || s[i] == '_'))
        sp.grouping = s[i++];

    if (i < s.size() && s[i] == '.') {
        i++;
        if (i >= s.size() || !is_dig(s[i]))
            return err_set("ValueError", "Format specifier missing precision"), false;
        i64 p = 0;
        while (i < s.size() && is_dig(s[i])) {
            p = p * 10 + (s[i++] - '0');
            if (p > 0x7fffff)
                return err_set("ValueError", "too many decimal digits in format string"), false;
        }
        sp.precision = i32(p);
    }

    if (i < s.size()) {
        sp.type = s[i++];
        if (i != s.size())
            return bad_spec(s, who), false;
    }
    out = sp;
    return true;
}

// -------------------------------------------------------------- the padding

namespace {

R put_fill(String &out, u32 cp, i32 n)
{
    char tmp[4];
    usize w = cp_encode(cp, tmp);
    for (i32 k = 0; k < n; k++)
        if (!out.append(Str(tmp, w)))
            return oom();
    return R::Ok;
}

u32 char_count(Str s)
{
    u32 n = 0;
    for (usize i = 0; i < s.size(); i++)
        if ((u8(s[i]) & 0xc0) != 0x80)
            n++;
    return n;
}

} // namespace

R format_pad(Str body, const Spec &s, char def_align, String &out)
{
    char align = s.align ? s.align : def_align;
    i32 have   = i32(char_count(body));
    i32 pad    = s.width > have ? s.width - have : 0;
    if (!pad)
        return out.append(body) ? R::Ok : oom();

    if (align == '<')
        return out.append(body) && put_fill(out, s.fill, pad) == R::Ok ? R::Ok : oom();
    if (align == '>' || align == '=')
        return put_fill(out, s.fill, pad) == R::Ok && out.append(body) ? R::Ok : oom();
    // '^': the odd character goes on the right, as CPython does.
    i32 left = pad / 2;
    return put_fill(out, s.fill, left) == R::Ok && out.append(body) &&
                   put_fill(out, s.fill, pad - left) == R::Ok
               ? R::Ok
               : oom();
}

// ------------------------------------------------------------------- number

namespace {

// A number is assembled as sign ++ prefix ++ digits, and only '=' alignment --
// which a bare `0` also asks for -- puts the padding between them.
R pad_number(Str sign, Str prefix, Str digits, const Spec &s, String &out)
{
    char align = s.align ? s.align : '>';
    if (align != '=') {
        String body;
        if (!body.append(sign) || !body.append(prefix) || !body.append(digits))
            return oom();
        return format_pad(body.str(), s, '>', out);
    }
    i32 have = i32(sign.size() + prefix.size() + char_count(digits));
    i32 pad  = s.width > have ? s.width - have : 0;
    if (!out.append(sign) || !out.append(prefix))
        return oom();
    if (put_fill(out, s.fill, pad) != R::Ok)
        return R::Err;
    return out.append(digits) ? R::Ok : oom();
}

// How many characters `n` digits become once separators are in.
i32 grouped_len(i32 n, i32 group)
{
    return n + (n - 1) / group;
}

// `digits` with a separator every `group`, padded on the left with zeros
// until the whole is at least `least` characters. A separator never leads.
R group_digits(Str digits, char sep, i32 group, i32 least, String &out)
{
    i32 n = i32(digits.size());
    if (sep) {
        while (grouped_len(n, group) < least)
            n++;
    } else if (n < least) {
        n = least;
    }
    i32 zeros = n - i32(digits.size());

    // One pass over the n digit positions, most significant first.
    for (i32 k = 0; k < n; k++) {
        if (k && sep && (n - k) % group == 0)
            if (!out.push(sep))
                return oom();
        char c = k < zeros ? '0' : digits[usize(k - zeros)];
        if (!out.push(c))
            return oom();
    }
    return R::Ok;
}

R format_int_by(Value v, const Spec &s, String &out)
{
    u32 base   = 10;
    Str prefix = "";
    bool upper = false;
    switch (s.type) {
    case 'b':
        base = 2, prefix = "0b";
        break;
    case 'o':
        base = 8, prefix = "0o";
        break;
    case 'x':
        base = 16, prefix = "0x";
        break;
    case 'X':
        base = 16, prefix = "0X", upper = true;
        break;
    default:
        break;
    }
    if (!s.alt)
        prefix = "";

    if (s.precision >= 0)
        return err_set("ValueError", "Precision not allowed in integer format specifier");

    // Any width: bigint.cpp writes the magnitude, whatever it takes.
    String text;
    if (int_digits(v, base, upper, text) != R::Ok)
        return R::Err;
    Str digits = text.str();

    bool neg = int_is_neg(v);
    Str sign = neg ? Str("-") : s.sign == '+' ? Str("+") : s.sign == ' ' ? Str(" ") : Str("");

    char sep = s.grouping;
    if (sep == ',' && s.type == 'n')
        return err_set("ValueError", "Cannot specify ',' with 'n'.");
    if (sep == ',' && base != 10) {
        Buf<64> b;
        b.put("Cannot specify ',' with '").put(s.type).put("'.");
        return err_set("ValueError", b.str());
    }

    // A bare `0` fills the digit field; any other alignment pads outside it.
    // Zeros fill the digit field itself -- and are grouped with it -- only
    // where the fill really is '0'; any other '=' pads with that character,
    // which pad_number does.
    i32 zero_least = 0;
    if (s.align == '=' && s.fill == '0' && s.width > 0)
        zero_least = s.width - i32(sign.size() + prefix.size());
    i32 least = zero_least > s.min_digits ? zero_least : s.min_digits;

    // `_` groups a radix by four and decimal by three, which is how the
    // digits of each are actually read.
    String field;
    if (group_digits(digits, sep, base == 10 ? 3 : 4, least, field) != R::Ok)
        return R::Err;

    Spec inner = s;
    if (zero_least > 0)
        inner.width = -1; // the zeros already filled it
    return pad_number(sign, prefix, field.str(), inner, out);
}

R format_char(i64 v, const Spec &s, String &out)
{
    if (s.sign || s.alt || s.grouping)
        return err_set("ValueError", "Cannot specify sign, '#' or grouping with 'c'");
    if (v < 0 || v > 0x10ffff)
        return err_set("OverflowError", "%c arg not in range(0x110000)");
    char tmp[4];
    usize w = cp_encode(u32(v), tmp);
    return format_pad(Str(tmp, w), s, '>', out);
}

// inf and nan take the alignment but no sign padding and no precision.
bool special_float(f64 x, char type, String &out, R &r)
{
    if (!isnan(x) && !isinf(x))
        return false;
    bool up = type == 'E' || type == 'F' || type == 'G';
    Str text;
    if (isnan(x))
        text = up ? Str("NAN") : Str("nan");
    else
        text = up ? Str("INF") : Str("inf");
    r = out.append(text) ? R::Ok : oom();
    return true;
}

R format_float_by(f64 x, const Spec &s, String &out)
{
    char type = s.type;
    i32 prec  = s.precision;
    if (type == '%')
        x *= 100;

    Str sign = "";
    if (!isnan(x)) {
        if (x < 0 || (x == 0 && 1 / x < 0))
            sign = "-";
        else if (s.sign == '+')
            sign = "+";
        else if (s.sign == ' ')
            sign = " ";
    } else if (s.sign == '+') {
        sign = "+";
    } else if (s.sign == ' ') {
        sign = " ";
    }

    String digits;
    R r = R::Ok;
    if (special_float(x, type, digits, r)) {
        if (r != R::Ok)
            return r;
        Spec plain  = s;
        plain.zero  = false;
        plain.fill  = s.align || !s.zero ? s.fill : ' ';
        plain.align = s.align ? s.align : '>';
        return pad_number(sign, "", digits.str(), plain, out);
    }

    char style = type;
    if (type == 0) {
        // No type at all is repr's shortest round-trip, not %g's six digits.
        char tmp[48];
        Str text = float_text(tmp, sizeof tmp, fabs(x));
        if (!digits.append(text))
            return oom();
    } else {
        if (type == '%')
            style = 'f';
        else if (type == 'n')
            style = 'g';
        if (prec < 0)
            prec = 6;
        // %g of an explicit zero precision is one significant digit.
        if ((style == 'g' || style == 'G') && prec == 0)
            prec = 1;
        char tmp[400];
        // '#' is C's: a decimal point always, and %g keeps its trailing
        // zeros. The engine underneath takes the flag, so it is handed over
        // rather than reproduced here.
        Str text = s.alt ? fmt_f64_padded(tmp, sizeof tmp, fabs(x), prec, style, 0, "#")
                         : fmt_f64(tmp, sizeof tmp, fabs(x), prec, style);
        if (!digits.append(text))
            return oom();
        if (type == '%' && !digits.push('%'))
            return oom();
    }

    // Grouping applies to the integer part alone.
    String field;
    if (s.grouping) {
        Str d      = digits.str();
        usize stop = 0;
        while (stop < d.size() && d[stop] >= '0' && d[stop] <= '9')
            stop++;
        if (group_digits(d.substr(0, stop), s.grouping, 3, 0, field) != R::Ok)
            return R::Err;
        if (!field.append(d.substr(stop)))
            return oom();
    } else if (!field.append(digits.str())) {
        return oom();
    }

    i32 least = 0;
    if (s.align == '=' && s.fill == '0' && s.width > 0)
        least = s.width - i32(sign.size());
    if (least > i32(char_count(field.str()))) {
        // Zero-fill the integer part, keeping any separators it already has.
        String wide;
        Str d      = field.str();
        usize stop = 0;
        while (stop < d.size() && (is_dig(d[stop]) || d[stop] == s.grouping))
            stop++;
        i32 rest = i32(char_count(d.substr(stop)));
        String ints;
        for (usize k = 0; k < stop; k++)
            if (is_dig(d[k]) && !ints.push(d[k]))
                return oom();
        if (group_digits(ints.str(), s.grouping, 3, least - rest, wide) != R::Ok)
            return R::Err;
        if (!wide.append(d.substr(stop)))
            return oom();
        Spec inner  = s;
        inner.width = -1;
        return pad_number(sign, "", wide.str(), inner, out);
    }
    return pad_number(sign, "", field.str(), s, out);
}

R format_string(Str text, const Spec &s, Str who, String &out)
{
    if (s.sign || s.alt || s.grouping || (s.type && s.type != 's'))
        return bad_spec("", who), R::Err;
    // A '0' before the width is a fill here and aligns nothing, since 3.10.
    Spec t = s;
    if (t.implied)
        t.align = 0;
    if (t.align == '=')
        return err_set("ValueError", "'=' alignment not allowed in string format specifier");
    if (s.precision >= 0) {
        // Cut at a character, never inside one.
        usize i = 0;
        i32 n   = 0;
        while (i < text.size() && n < s.precision) {
            usize step = 0;
            rune_at(text, i, step);
            i += step;
            n++;
        }
        text = text.substr(0, i);
    }
    return format_pad(text, t, '<', out);
}

} // namespace

R format_builtin(Value v, Str spec, String &out, i32 min_digits)
{
    Str who = type_name(v);
    Spec s;
    if (!spec_parse(spec, s, who))
        return R::Err;
    s.min_digits = min_digits;

    if (is_str(v)) {
        if (s.type && s.type != 's')
            return bad_spec(spec, who), R::Err;
        return format_string(str_of(v)->str(), s, who, out);
    }

    // bool with a numeric type is an int; with none it is its own name.
    if (is_intval(v)) {
        if (is_bool(v) && !s.type && s.width < 0 && !s.align)
            return py_str(v, out);
        switch (s.type) {
        case 0:
        case 'd':
        case 'n':
        case 'b':
        case 'o':
        case 'x':
        case 'X':
            return format_int_by(v, s, out);
        case 'c': {
            i64 n = 0;
            if (!int_to_i64(v, n))
                return err_set("OverflowError", "%c arg not in range(0x110000)");
            return format_char(n, s, out);
        }
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case '%': {
            Spec f = s;
            if (f.type == 0)
                f.type = 'g';
            return format_float_by(int_to_f64(v), f, out);
        }
        default:
            return bad_spec(spec, who), R::Err;
        }
    }

    if (is_float(v)) {
        switch (s.type) {
        case 0:
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'n':
        case '%':
            return format_float_by(float_of(v), s, out);
        default:
            return bad_spec(spec, who), R::Err;
        }
    }

    // Everything else is object.__format__: str(v), and only an empty spec.
    if (!spec.size())
        return py_str(v, out);
    Buf<96> b;
    b.put("unsupported format string passed to ").put(who).put(".__format__");
    return err_set("TypeError", b.str());
}

R py_format(Value v, Str spec, String &out)
{
    return format_builtin(v, spec, out);
}

R py_ascii(Value v, String &out)
{
    String text;
    if (py_repr(v, text) != R::Ok)
        return R::Err;
    Str s = text.str();
    for (usize i = 0; i < s.size();) {
        u8 c = u8(s[i]);
        if (c < 0x80) {
            if (!out.push(char(c)))
                return oom();
            i++;
            continue;
        }
        usize used = 0;
        u32 x      = rune_at(s, i, used);
        i += used;
        if (put_escape(out, x) != R::Ok)
            return R::Err;
    }
    return R::Ok;
}
