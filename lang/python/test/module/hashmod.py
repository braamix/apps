# The native hashes under hashlib: every size of block, chunked updates, BLAKE2's parameters.
import _md5, _sha1, _sha2, _sha3, _blake2
ctors = [_md5.md5, _sha1.sha1, _sha2.sha224, _sha2.sha256, _sha2.sha384, _sha2.sha512,
         _sha3.sha3_224, _sha3.sha3_256, _sha3.sha3_384, _sha3.sha3_512, _blake2.blake2b, _blake2.blake2s]
data = bytes((i * 7 + 3) & 255 for i in range(700))
for c in ctors:
    h = c()
    print(h.name, h.digest_size, h.block_size, c(b"").hexdigest())
    for n in (1, 3, 55, 56, 57, 63, 64, 65, 111, 112, 119, 120, 127, 128, 129, 200, 700):
        print(n, c(data[:n]).hexdigest())
    h = c()
    for i in range(0, 700, 37):
        h.update(data[i:i + 37])
    g = h.copy()
    g.update(b"x")
    print("chunks", h.hexdigest() == c(data).hexdigest(), g.hexdigest() != h.hexdigest(), h.digest()[:4])
for c in (_sha3.shake_128, _sha3.shake_256):
    h = c(data[:100])
    print(h.name, h.digest_size, h.block_size, h._rate_bits, h._capacity_bits, h._suffix)
    print(h.hexdigest(0), h.hexdigest(10), h.hexdigest(300)[-20:], h.digest(5))
print(_blake2.blake2b(b"abc", digest_size=20, key=b"k" * 64, salt=b"s" * 16, person=b"p" * 16, fanout=2, depth=3, leaf_size=4096, node_offset=2**63, node_depth=7, inner_size=64, last_node=True).hexdigest())
print(_blake2.blake2s(b"abc", digest_size=17, key=b"k" * 32, salt=b"s" * 8, person=b"p" * 8, fanout=0, depth=255, leaf_size=2**32-1, node_offset=2**48-1, node_depth=255, inner_size=32, last_node=True).hexdigest())
print(_blake2.blake2b(key=b"key").hexdigest(), _blake2.blake2s(data[:64], key=b"key").hexdigest())
print(_blake2.blake2b.MAX_DIGEST_SIZE, _blake2.BLAKE2S_SALT_SIZE, _blake2.blake2s.PERSON_SIZE)
for bad in [dict(digest_size=0), dict(digest_size=65), dict(key=b"x"*65), dict(fanout=256), dict(depth=0), dict(leaf_size=2**32), dict(leaf_size=-1), dict(salt=b"x"*17), dict(inner_size=65)]:
    try: _blake2.blake2b(**bad)
    except (ValueError, OverflowError) as e: print(type(e).__name__, e)
try: _blake2.blake2s(node_offset=2**48)
except OverflowError as e: print(e)
for bad in ["abc", 5, [1]]:
    try: _md5.md5(bad)
    except TypeError as e: print(e)
try: _sha3.shake_128().digest(-1)
except ValueError as e: print(e)
try: _md5.md5(b"a", b"b")
except TypeError as e: print(e)
try: _md5.md5(b"a", data=b"b")
except TypeError as e: print(e)
print(_md5.md5(data=b"abc").hexdigest())
print(_md5.MD5Type is type(_md5.md5()), _sha2.SHA512Type is type(_sha2.sha512()), _sha3.implementation, _md5._GIL_MINSIZE)
import array
print(_sha1.sha1(memoryview(b"abcdef")[1:4]).hexdigest(), _sha1.sha1(array.array("I", [1, 2])).hexdigest(), _sha1.sha1(bytearray(b"bcd")).hexdigest())
try:
    class X(_sha3.sha3_224): pass
except TypeError as e: print(e)
