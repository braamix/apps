// OutputBuffer, which the demangler prints into.
// From LLVM 23.1.2, less what llvm::demangle() does not reach.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "std.h"

namespace llvm::itanium_demangle {

class Node;

inline bool starts_with(std::string_view self, char C) noexcept
{
    return !self.empty() && *self.begin() == C;
}

inline bool starts_with(std::string_view haystack, std::string_view needle) noexcept
{
    if (needle.size() > haystack.size())
        return false;
    haystack.remove_suffix(haystack.size() - needle.size());
    return haystack == needle;
}

// Stream that AST nodes write their string representation into after the AST
// has been parsed.
class OutputBuffer {
    char *Buffer           = nullptr;
    size_t CurrentPosition = 0;
    size_t BufferCapacity  = 0;

    // Ensure there are at least N more positions in the buffer.
    void grow(size_t N)
    {
        size_t Need = N + CurrentPosition;
        if (Need > BufferCapacity) {
            // Reduce the number of reallocations, with a bit of hysteresis. The
            // number here is chosen so the first allocation will more-than-likely not
            // allocate more than 1K.
            Need += 1024 - 32;
            BufferCapacity *= 2;
            if (BufferCapacity < Need)
                BufferCapacity = Need;
            Buffer = static_cast<char *>(std::realloc(Buffer, BufferCapacity));
            if (Buffer == nullptr)
                std::abort();
        }
    }

    OutputBuffer &writeUnsigned(uint64_t N)
    {
        std::array<char, 21> Temp;
        char *TempPtr = Temp.data() + Temp.size();

        // Output at least one character.
        do {
            *--TempPtr = char('0' + N % 10);
            N /= 10;
        } while (N);

        return operator+=(std::string_view(TempPtr, Temp.data() + Temp.size() - TempPtr));
    }

public:
    OutputBuffer() = default;
    // Non-copyable
    OutputBuffer(const OutputBuffer &)            = delete;
    OutputBuffer &operator=(const OutputBuffer &) = delete;

    /// Called by the demangler when printing the demangle tree.
    void printLeft(const Node &N);
    void printRight(const Node &N);

    /// If a ParameterPackExpansion (or similar type) is encountered, the offset
    /// into the pack that we're currently printing.
    unsigned CurrentPackIndex = std::numeric_limits<unsigned>::max();
    unsigned CurrentPackMax   = std::numeric_limits<unsigned>::max();

    struct {
        /// The depth of '(' and ')' inside the currently printed template
        /// arguments.
        unsigned ParenDepth = 0;

        /// True if we're currently printing a template argument.
        bool InsideTemplate = false;
    } TemplateTracker;

    /// Returns true if we're currently between a '(' and ')' when printing
    /// template args.
    bool isInParensInTemplateArgs() const { return TemplateTracker.ParenDepth > 0; }

    /// Returns true if we're printing template args.
    bool isInsideTemplateArgs() const { return TemplateTracker.InsideTemplate; }

    void printOpen(char Open = '(')
    {
        if (isInsideTemplateArgs())
            TemplateTracker.ParenDepth++;
        *this += Open;
    }
    void printClose(char Close = ')')
    {
        if (isInsideTemplateArgs())
            TemplateTracker.ParenDepth--;
        *this += Close;
    }

    OutputBuffer &operator+=(std::string_view R)
    {
        if (size_t Size = R.size()) {
            grow(Size);
            std::memcpy(Buffer + CurrentPosition, &*R.begin(), Size);
            CurrentPosition += Size;
        }
        return *this;
    }

    OutputBuffer &operator+=(char C)
    {
        grow(1);
        Buffer[CurrentPosition++] = C;
        return *this;
    }

    OutputBuffer &operator<<(std::string_view R) { return (*this += R); }

    OutputBuffer &operator<<(char C) { return (*this += C); }

    OutputBuffer &operator<<(unsigned long long N) { return writeUnsigned(N); }

    OutputBuffer &operator<<(unsigned int N)
    {
        return this->operator<<(static_cast<unsigned long long>(N));
    }

    size_t getCurrentPosition() const { return CurrentPosition; }
    void setCurrentPosition(size_t NewPos) { CurrentPosition = NewPos; }

    char back() const { return Buffer[CurrentPosition - 1]; }

    char *getBuffer() { return Buffer; }
};

template <class T>
class ScopedOverride {
    T &Loc;
    T Original;

public:
    ScopedOverride(T &Loc_) : ScopedOverride(Loc_, Loc_) {}

    ScopedOverride(T &Loc_, T NewVal) : Loc(Loc_), Original(Loc_) { Loc_ = std::move(NewVal); }
    ~ScopedOverride() { Loc = std::move(Original); }

    ScopedOverride(const ScopedOverride &)            = delete;
    ScopedOverride &operator=(const ScopedOverride &) = delete;
};

} // namespace llvm::itanium_demangle
