// The part of the C++ library LLVM's demangler uses, over the kernel's heap
// and headers: there is no other here.
#pragma once

#include "kernel/alloc.h"
#include "kernel/traits.h"
#include "kernel/types.h"

using size_t    = usize;
using uint8_t   = u8;
using uint16_t  = u16;
using uint32_t  = u32;
using uint64_t  = u64;
using int64_t   = i64;
using uintptr_t = usize;

namespace std {

using ::int64_t;
using ::size_t;
using ::uint16_t;
using ::uint32_t;
using ::uint64_t;
using ::uint8_t;

template <class T>
constexpr __remove_reference_t(T) &&move(T &&v) noexcept
{
    return static_cast<__remove_reference_t(T) &&>(v);
}

template <class T>
constexpr T &&forward(__remove_reference_t(T) &v) noexcept
{
    return static_cast<T &&>(v);
}

template <class T>
constexpr T &&forward(__remove_reference_t(T) &&v) noexcept
{
    return static_cast<T &&>(v);
}

template <class T>
constexpr void swap(T &a, T &b) noexcept
{
    T t = std::move(a);
    a   = std::move(b);
    b   = std::move(t);
}

template <class T>
constexpr const T &min(const T &a, const T &b)
{
    return b < a ? b : a;
}

template <class T>
constexpr const T &max(const T &a, const T &b)
{
    return a < b ? b : a;
}

template <class I, class O>
O copy(I first, I last, O out)
{
    for (; first != last; ++first, ++out)
        *out = *first;
    return out;
}

template <class I, class P>
bool all_of(I first, I last, P p)
{
    for (; first != last; ++first)
        if (!p(*first))
            return false;
    return true;
}

template <class I>
void reverse(I first, I last)
{
    while (first != last && first != --last) {
        swap(*first, *last);
        ++first;
    }
}

template <class A, class B>
struct pair {
    A first;
    B second;
};

template <class A, class B>
constexpr pair<A, B> make_pair(A a, B b)
{
    return { a, b };
}

template <class T, usize N>
struct array {
    T v[N];
    T *data() { return v; }
    T &operator[](usize i) { return v[i]; }
    T *begin() { return v; }
    T *end() { return v + N; }
    constexpr usize size() const { return N; }
};

template <class T>
struct numeric_limits;

template <>
struct numeric_limits<unsigned> {
    static constexpr unsigned max() { return ~0u; }
};

template <bool B, class T = void>
struct enable_if {};

template <class T>
struct enable_if<true, T> {
    using type = T;
};

template <bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;

template <class T>
struct is_trivially_copyable {
    static constexpr bool value = __is_trivially_copyable(T);
};

template <class T>
struct is_trivially_default_constructible {
    static constexpr bool value = __is_trivially_constructible(T);
};

template <class T>
struct is_signed {
    static constexpr bool value = T(-1) < T(0);
};

template <class T>
struct is_unsigned {
    static constexpr bool value = T(-1) > T(0);
};

inline long long abs(long long n)
{
    return n < 0 ? -n : n;
}

inline int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

constexpr usize strlen(const char *s)
{
    usize n = 0;
    while (s[n])
        n++;
    return n;
}

inline void *memcpy(void *d, const void *s, usize n)
{
    return __builtin_memcpy(d, s, n);
}

inline void *memmove(void *d, const void *s, usize n)
{
    return __builtin_memmove(d, s, n);
}

inline int memcmp(const void *a, const void *b, usize n)
{
    return __builtin_memcmp(a, b, n);
}

inline void *malloc(usize n)
{
    return heap_alloc(n ? n : 1);
}

inline void free(void *p)
{
    if (p)
        heap_free(p);
}

inline void *realloc(void *p, usize n)
{
    if (!p)
        return malloc(n);
    usize had = heap_usable_size(p);
    if (n <= had)
        return p;
    void *q = malloc(n);
    if (q) {
        memcpy(q, p, had);
        free(p);
    }
    return q;
}

[[noreturn]] inline void abort()
{
    __builtin_trap();
}

[[noreturn]] inline void terminate()
{
    __builtin_trap();
}

// std::string_view, as much of it as the demangler asks for.
class string_view {
    const char *p_ = nullptr;
    usize n_       = 0;

public:
    static constexpr usize npos = ~usize(0);
    using const_iterator        = const char *;

    constexpr string_view() = default;
    constexpr string_view(const char *p, usize n) : p_(p), n_(n) {}
    constexpr string_view(const char *s) : p_(s), n_(strlen(s)) {}
    constexpr string_view(const char *b, const char *e) : p_(b), n_(usize(e - b)) {}

    constexpr const char *data() const { return p_; }
    constexpr usize size() const { return n_; }
    constexpr usize length() const { return n_; }
    constexpr bool empty() const { return n_ == 0; }
    constexpr const char *begin() const { return p_; }
    constexpr const char *end() const { return p_ + n_; }
    constexpr char operator[](usize i) const { return p_[i]; }
    constexpr char front() const { return p_[0]; }
    constexpr char back() const { return p_[n_ - 1]; }
    constexpr void remove_prefix(usize k) { p_ += k, n_ -= k; }
    constexpr void remove_suffix(usize k) { n_ -= k; }

    constexpr string_view substr(usize at, usize k = npos) const
    {
        usize rest = n_ - at;
        return string_view(p_ + at, k < rest ? k : rest);
    }

    // Only rbegin's address is taken: the last character's.
    constexpr const char *rbegin() const { return p_ + n_ - 1; }

    constexpr usize find(char c) const
    {
        for (usize i = 0; i < n_; i++)
            if (p_[i] == c)
                return i;
        return npos;
    }

    constexpr usize find_first_not_of(char c) const
    {
        for (usize i = 0; i < n_; i++)
            if (p_[i] != c)
                return i;
        return npos;
    }

    friend constexpr bool operator==(string_view a, string_view b)
    {
        if (a.n_ != b.n_)
            return false;
        for (usize i = 0; i < a.n_; i++)
            if (a.p_[i] != b.p_[i])
                return false;
        return true;
    }
};

} // namespace std

using std::isdigit;

// The float literal printer's one use of the C library: %a, %af and %LaL.
int snprintf(char *buf, usize n, const char *spec, ...);
