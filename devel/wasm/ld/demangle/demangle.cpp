// llvm/lib/Demangle/ItaniumDemangle.cpp and Demangle.cpp of LLVM 23.1.2,
// the part llvm::demangle() reaches for an Itanium name: the rest (the
// partial demangler, dumping, Rust, D and Microsoft names) is not here.

#include "demangle.h"

#include "ItaniumDemangle.h"

using namespace llvm;
using namespace llvm::itanium_demangle;

// <discriminator> := _ <non-negative number>      # when number < 10
//                 := __ <non-negative number> _   # when number >= 10
//  extension      := decimal-digit+               # at the end of string
const char *itanium_demangle::parse_discriminator(const char *first, const char *last)
{
    // parse but ignore discriminator
    if (first != last) {
        if (*first == '_') {
            const char *t1 = first + 1;
            if (t1 != last) {
                if (std::isdigit(*t1))
                    first = t1 + 1;
                else if (*t1 == '_') {
                    for (++t1; t1 != last && std::isdigit(*t1); ++t1)
                        ;
                    if (t1 != last && *t1 == '_')
                        first = t1 + 1;
                }
            }
        } else if (std::isdigit(*first)) {
            const char *t1 = first + 1;
            for (; t1 != last && std::isdigit(*t1); ++t1)
                ;
            if (t1 == last)
                first = last;
        }
    }
    return first;
}

namespace {

class BumpPointerAllocator {
    struct BlockMeta {
        BlockMeta *Next;
        size_t Current;
    };

    static constexpr size_t AllocSize       = 4096;
    static constexpr size_t UsableAllocSize = AllocSize - sizeof(BlockMeta);

    alignas(long double) char InitialBuffer[AllocSize];
    BlockMeta *BlockList = nullptr;

    void grow()
    {
        char *NewMeta = static_cast<char *>(std::malloc(AllocSize));
        if (NewMeta == nullptr)
            std::terminate();
        BlockList = new (NewMeta) BlockMeta{ BlockList, 0 };
    }

    void *allocateMassive(size_t NBytes)
    {
        NBytes += sizeof(BlockMeta);
        BlockMeta *NewMeta = reinterpret_cast<BlockMeta *>(std::malloc(NBytes));
        if (NewMeta == nullptr)
            std::terminate();
        BlockList->Next = new (NewMeta) BlockMeta{ BlockList->Next, 0 };
        return static_cast<void *>(NewMeta + 1);
    }

public:
    BumpPointerAllocator() : BlockList(new (InitialBuffer) BlockMeta{ nullptr, 0 }) {}

    void *allocate(size_t N)
    {
        N = (N + 15u) & ~15u;
        if (N + BlockList->Current >= UsableAllocSize) {
            if (N > UsableAllocSize)
                return allocateMassive(N);
            grow();
        }
        BlockList->Current += N;
        return static_cast<void *>(reinterpret_cast<char *>(BlockList + 1) + BlockList->Current -
                                   N);
    }

    void reset()
    {
        while (BlockList) {
            BlockMeta *Tmp = BlockList;
            BlockList      = BlockList->Next;
            if (reinterpret_cast<char *>(Tmp) != InitialBuffer)
                std::free(Tmp);
        }
        BlockList = new (InitialBuffer) BlockMeta{ nullptr, 0 };
    }

    ~BumpPointerAllocator() { reset(); }
};

class DefaultAllocator {
    BumpPointerAllocator Alloc;

public:
    void reset() { Alloc.reset(); }

    template <typename T, typename... Args>
    T *makeNode(Args &&...args)
    {
        return new (Alloc.allocate(sizeof(T))) T(std::forward<Args>(args)...);
    }

    void *allocateNodeArray(size_t sz) { return Alloc.allocate(sizeof(Node *) * sz); }
};

using Demangler = ManglingParser<DefaultAllocator>;

// llvm::itaniumDemangle, the parser on the heap: it is some kilobytes.
char *itanium_demangle_name(std::string_view MangledName, bool ParseParams)
{
    if (MangledName.empty())
        return nullptr;
    Demangler *Parser =
        heap_new<Demangler>(MangledName.data(), MangledName.data() + MangledName.length());
    if (!Parser)
        return nullptr;
    Node *AST    = Parser->parse(ParseParams);
    char *Result = nullptr;
    if (AST) {
        OutputBuffer OB;
        AST->print(OB);
        OB += '\0';
        Result = OB.getBuffer();
    }
    heap_delete(Parser);
    return Result;
}

bool starts_with(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

bool isItaniumEncoding(std::string_view S)
{
    if (starts_with(S, "__alloc_token_")) {
        S.remove_prefix(sizeof("__alloc_token_") - 1);
        if (!S.empty() && std::isdigit(S[0])) {
            while (!S.empty() && std::isdigit(S[0]))
                S.remove_prefix(1);
            if (starts_with(S, "_"))
                S.remove_prefix(1);
        }
    }
    // Itanium demangler supports prefixes with 1-4 underscores.
    const size_t Pos = S.find_first_not_of('_');
    return Pos > 0 && Pos <= 4 && Pos != std::string_view::npos && S[Pos] == 'Z';
}

// llvm::nonMicrosoftDemangle, for Itanium names alone.
bool nonMicrosoftDemangle(std::string_view MangledName, String &Result, bool CanHaveLeadingDot)
{
    // Do not consider the dot prefix as part of the demangled symbol name.
    if (CanHaveLeadingDot && MangledName.size() > 0 && MangledName[0] == '.') {
        MangledName.remove_prefix(1);
        Result.assign(Str(".", 1));
    }
    char *Demangled = nullptr;
    if (isItaniumEncoding(MangledName))
        Demangled = itanium_demangle_name(MangledName, true);
    if (!Demangled)
        return false;
    Result.append(Str(Demangled, std::strlen(Demangled)));
    std::free(Demangled);
    return true;
}

} // namespace

void demangle(Str name, String &out)
{
    // llvm::demangle: the name, else the name less one underscore; one
    // Result for both, so a dot the first attempt set aside stays.
    std::string_view s(name.data(), name.size());
    out.clear();
    if (nonMicrosoftDemangle(s, out, true))
        return;
    if (!name.empty() && name[0] == '_' && nonMicrosoftDemangle(s.substr(1), out, false))
        return;
    out.assign(name);
}

// ------------------------------------------------------------ %a

namespace {

// A double as %a writes it: 0x1.<hex>p<exponent>, trailing zeros dropped.
usize hex_double(char *buf, usize n, double v, char suffix)
{
    u64 bits;
    __builtin_memcpy(&bits, &v, 8);
    char tmp[40];
    usize k = 0;
    if (bits >> 63)
        tmp[k++] = '-';
    i32 exp  = i32((bits >> 52) & 0x7ff);
    u64 frac = bits & ((u64(1) << 52) - 1);
    if (exp == 0x7ff) {
        const char *w = frac ? "nan" : "inf";
        for (; *w; w++)
            tmp[k++] = *w;
    } else {
        tmp[k++] = '0';
        tmp[k++] = 'x';
        if (exp == 0 && frac == 0) {
            tmp[k++] = '0';
            exp      = 0;
        } else {
            if (exp == 0) {
                // Subnormal: normalized, as the BSD printf does.
                exp = 1;
                while (!(frac & (u64(1) << 52))) {
                    frac <<= 1;
                    exp--;
                }
                frac &= (u64(1) << 52) - 1;
            }
            exp -= 1023;
            tmp[k++] = '1';
            if (frac) {
                tmp[k++]   = '.';
                int digits = 13;
                while (!(frac & 0xf)) {
                    frac >>= 4;
                    digits--;
                }
                for (int d = digits - 1; d >= 0; d--)
                    tmp[k++] = "0123456789abcdef"[(frac >> (4 * d)) & 0xf];
            }
        }
        tmp[k++] = 'p';
        tmp[k++] = exp < 0 ? '-' : '+';
        u32 e    = u32(exp < 0 ? -exp : exp);
        char d[8];
        int m = 0;
        do
            d[m++] = char('0' + e % 10);
        while (e /= 10);
        while (m)
            tmp[k++] = d[--m];
    }
    if (suffix)
        tmp[k++] = suffix;
    usize w = k < n ? k : n ? n - 1 : 0;
    for (usize i = 0; i < w; i++)
        buf[i] = tmp[i];
    if (n)
        buf[w] = 0;
    return k;
}

} // namespace

int snprintf(char *buf, usize n, const char *spec, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, spec);
    int k = 0;
    // %LaL: a long double here is 113 bits, wasm-ld's host one 53, so no
    // mangled literal reads alike in both; it prints as nothing.
    if (spec[1] == 'a')
        k = int(hex_double(buf, n, __builtin_va_arg(ap, double), spec[2]));
    else if (n)
        buf[0] = 0;
    __builtin_va_end(ap);
    return k;
}
