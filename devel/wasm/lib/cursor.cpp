#include "cursor.h"

void Cursor::fail(Str why, usize at)
{
    if (!failed_) {
        failed_ = true;
        why_    = why;
        where_  = at;
    }
    at_ = b_.size();
}

u8 Cursor::byte()
{
    if (at_ >= b_.size()) {
        fail("unexpected end of section");
        return 0;
    }
    return b_[at_++];
}

u32 Cursor::u32le()
{
    if (left() < 4) {
        fail("unexpected end of section");
        return 0;
    }
    u32 v = u32(b_[at_]) | u32(b_[at_ + 1]) << 8 | u32(b_[at_ + 2]) << 16 | u32(b_[at_ + 3]) << 24;
    at_ += 4;
    return v;
}

u32 Cursor::uleb()
{
    usize start = at_;
    u32 v       = 0;
    for (u32 i = 0; i < 5; i++) {
        if (at_ >= b_.size()) {
            fail("unexpected end of section", start);
            return 0;
        }
        u8 x = b_[at_++];
        v |= u32(x & 0x7f) << (7 * i);
        if (!(x & 0x80)) {
            if (i == 4 && (x & 0x70)) {
                fail("LEB does not fit 32 bits", start);
                return 0;
            }
            return v;
        }
    }
    fail("LEB longer than 5 bytes", start);
    return 0;
}

i32 Cursor::sleb()
{
    usize start = at_;
    u32 v       = 0;
    for (u32 i = 0; i < 5; i++) {
        if (at_ >= b_.size()) {
            fail("unexpected end of section", start);
            return 0;
        }
        u8 x = b_[at_++];
        v |= u32(x & 0x7f) << (7 * i);
        if (!(x & 0x80)) {
            if (i == 4) {
                u8 top = x & 0x78; // bit 3 is the sign; 4-6 must repeat it
                if (top != 0 && top != 0x78) {
                    fail("LEB does not fit 32 bits", start);
                    return 0;
                }
            } else if (x & 0x40) {
                v |= ~u32(0) << (7 * (i + 1));
            }
            return i32(v);
        }
    }
    fail("LEB longer than 5 bytes", start);
    return 0;
}

i64 Cursor::sleb64()
{
    usize start = at_;
    u64 v       = 0;
    for (u32 i = 0; i < 10; i++) {
        if (at_ >= b_.size()) {
            fail("unexpected end of section", start);
            return 0;
        }
        u8 x = b_[at_++];
        v |= u64(x & 0x7f) << (7 * i);
        if (!(x & 0x80)) {
            if (i == 9) {
                if (x != 0 && x != 0x7f) {
                    fail("LEB does not fit 64 bits", start);
                    return 0;
                }
            } else if (x & 0x40) {
                v |= ~u64(0) << (7 * (i + 1));
            }
            return i64(v);
        }
    }
    fail("LEB longer than 10 bytes", start);
    return 0;
}

Bytes Cursor::take(usize n)
{
    if (n > left()) {
        fail("unexpected end of section");
        return Bytes();
    }
    Bytes b = b_.subspan(at_, n);
    at_ += n;
    return b;
}

Str Cursor::name()
{
    u32 n   = uleb();
    Bytes b = take(n);
    return Str(reinterpret_cast<const char *>(b.data()), b.size());
}

u32 Cursor::count()
{
    usize start = at_;
    u32 n       = uleb();
    if (n > left()) {
        fail("count is larger than the section", start);
        return 0;
    }
    return n;
}
