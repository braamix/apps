"""The Citrus hashed database container.

citrus_db_factory.c and citrus_db_hash.c, in Python. Every .mps, .esdb, .db
and .pvdb is one of these: a header, a bucket array, a key table, a data table.
All words big-endian.

The layout has to come out byte for byte, so the two passes that place an entry
are upstream's, not an equivalent of them.
"""

DB_ALIGN = 16
HEADER_SIZE = 16
ENTRY_SIZE = 24
MAGIC_SIZE = 8


def ceilto(n):
    return (n + DB_ALIGN - 1) & ~(DB_ALIGN - 1)


def tolower(c):
    # _bcs_tolower: ASCII only, whatever the locale.
    return c + 32 if 0x41 <= c <= 0x5A else c


def hash_std(key):
    """citrus_db_hash.c: ELF's hash over the case-folded key."""
    h = 0
    for c in key:
        h = ((h << 4) + tolower(c)) & 0xFFFFFFFF
        tmp = h & 0xF0000000
        if tmp:
            h ^= tmp
            h ^= tmp >> 24
    return h


def u32(v):
    return v.to_bytes(4, "big")


class DbFactory:
    """Entries in insertion order; that order decides the collision chains."""

    def __init__(self):
        self.entries = []  # (key bytes, data bytes)

    def add(self, key, data):
        self.entries.append((key, data))

    def add_by_string(self, key, data):
        self.add(key.encode(), data)

    def add8(self, key, val):
        self.add_by_string(key, bytes([val]))

    def add16(self, key, val):
        self.add_by_string(key, val.to_bytes(2, "big"))

    def add32(self, key, val):
        self.add_by_string(key, u32(val))

    def add_string(self, key, data):
        # The data carries its NUL; the key does not.
        self.add_by_string(key, data.encode() + b"\0")

    def calc_size(self):
        n = len(self.entries)
        keys = sum(len(k) for k, _ in self.entries)
        data = sum(ceilto(len(d)) for _, d in self.entries)
        return ceilto(HEADER_SIZE) + ceilto(ENTRY_SIZE * n) + ceilto(keys) + data

    def serialize(self, magic):
        assert len(magic) == MAGIC_SIZE, magic
        n = len(self.entries)
        out = bytearray(self.calc_size())
        out[0:MAGIC_SIZE] = magic

        if n == 0:
            out[MAGIC_SIZE:MAGIC_SIZE + 4] = u32(0)
            out[MAGIC_SIZE + 4:HEADER_SIZE] = u32(HEADER_SIZE)
            return bytes(out)

        hashes = [hash_std(k) % n for k, _ in self.entries]

        # Pass 1: an entry whose own bucket is free takes it.
        slot = [None] * n           # bucket -> entry index
        idx = [-1] * n              # entry index -> bucket
        for e, h in enumerate(hashes):
            if slot[h] is None:
                slot[h] = e
                idx[e] = h

        # Pass 2: the rest go on the tail of their chain, and into the lowest
        # bucket still free. `i` does not restart between entries.
        nxt = [None] * n            # entry index -> entry index
        i = 0
        for e, h in enumerate(hashes):
            if idx[e] != -1:
                continue
            t = slot[h]
            while nxt[t] is not None:
                t = nxt[t]
            nxt[t] = e
            while slot[i] is not None:
                i += 1
            slot[i] = e
            idx[e] = i

        out[MAGIC_SIZE:MAGIC_SIZE + 4] = u32(n)
        out[MAGIC_SIZE + 4:HEADER_SIZE] = u32(HEADER_SIZE)

        keyofs = HEADER_SIZE + ceilto(n * ENTRY_SIZE)
        dataofs = keyofs + ceilto(sum(len(k) for k, _ in self.entries))

        ofs = HEADER_SIZE
        for bucket in range(n):
            e = slot[bucket]
            key, data = self.entries[e]
            nextofs = 0 if nxt[e] is None else HEADER_SIZE + idx[nxt[e]] * ENTRY_SIZE
            out[ofs:ofs + ENTRY_SIZE] = (
                u32(hashes[e]) + u32(nextofs) + u32(keyofs) + u32(len(key))
                + u32(dataofs) + u32(len(data)))
            ofs += ENTRY_SIZE

            out[keyofs:keyofs + len(key)] = key
            keyofs += len(key)
            out[dataofs:dataofs + len(data)] = data
            dataofs = ceilto(dataofs + len(data))

        return bytes(out)


def parse(blob):
    """The reader, for verify.py's diagnostics."""
    magic = bytes(blob[:MAGIC_SIZE])
    n = int.from_bytes(blob[MAGIC_SIZE:MAGIC_SIZE + 4], "big")
    entries = []
    for i in range(n):
        o = HEADER_SIZE + i * ENTRY_SIZE
        f = [int.from_bytes(blob[o + j * 4:o + j * 4 + 4], "big") for j in range(6)]
        h, nx, ko, ks, do, ds = f
        entries.append({
            "hash": h, "next": nx,
            "key": bytes(blob[ko:ko + ks]),
            "data": bytes(blob[do:do + ds]),
        })
    return magic, entries
