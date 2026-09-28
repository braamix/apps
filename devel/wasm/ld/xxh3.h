// LLVM's xxh3_64bits, which orders lld's hash tables, and the one of those
// tables whose order reaches the output.
#pragma once

#include "kernel/str.h"
#include "kernel/vec.h"

u64 xxh3_64(const u8 *in, usize len);

// DenseMapInfo<CachedHashStringRef>'s hash of a name, in a release build.
u32 lld_hash(Str s);

// The order a DenseMap iterates keys inserted in the order of `hashes`:
// linear probing over 64 buckets and up, doubled at three quarters full.
// Positions into `hashes`; false if out of memory.
bool dense_map_order(Span<const u32> hashes, Vec<u32> &order);
