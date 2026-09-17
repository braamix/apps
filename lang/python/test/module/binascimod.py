# binascii, natively: every codec, strict and lenient, and its errors.
import binascii as b
data = bytes(range(256)) + b"The quick brown fox"
for n in (0, 1, 2, 3, 4, 5, 7, 45):
    d = data[:n]
    print(n, b.b2a_base64(d), b.b2a_base64(d, newline=False, padded=False), b.b2a_base32(d), b.b2a_base85(d), b.b2a_ascii85(d, adobe=True, wrapcol=7))
    print(b.a2b_base64(b.b2a_base64(d)) == d, b.a2b_base32(b.b2a_base32(d)) == d, b.a2b_base85(b.b2a_base85(d)) == d, b.a2b_ascii85(b.b2a_ascii85(d, adobe=True), adobe=True) == d)
    print(b.b2a_uu(d, backtick=True), b.a2b_uu(b.b2a_uu(d)) == d, b.crc32(d), b.crc_hqx(d, 7), b.hexlify(d, '-', -3))
print(b.b2a_base64(data, wrapcol=30))
print(b.b2a_qp(b"hello = world \t\nmore.\n.\r\n" + bytes(range(0, 256, 17)) * 5, header=True))
print(b.a2b_qp(b"a=3Db=\nc=ZZ_d", header=True))
for bad in [b"a", b"ab=", b"=ab", b"ab==c", b"a@b=", b"abc=d"]:
    for strict in (False, True):
        try: print(bad, strict, b.a2b_base64(bad, strict_mode=strict))
        except b.Error as e: print(bad, strict, "Error", e)
for bad in [b"AB=", b"ABC=====", b"A", b"==", b"AB======C"]:
    try: print(bad, b.a2b_base32(bad))
    except b.Error as e: print(bad, "Error", e)
for bad in [b"z!", b"!!!!!", b"xyz", b"~>", b"s8W-\"~>"]:
    try: print(bad, b.a2b_ascii85(bad, adobe=True, canonical=True))
    except b.Error as e: print(bad, "Error", e)
print(b.unhexlify("de ad", ignorechars=b" "), b.a2b_hex(b"BEEF"))
for bad in ["abc", "zz", "é"]:
    try: print(b.unhexlify(bad))
    except (b.Error, ValueError) as e: print(type(e).__name__, e)
print(b.Error.__mro__, b.Incomplete.__module__, b.BASE32HEX_ALPHABET)
print(b.a2b_base64(b"-_-_", alphabet=b.URLSAFE_BASE64_ALPHABET), b.b2a_base32(b"hi", alphabet=b.BASE32HEX_ALPHABET))
print(b.crc32(b"abc", -1), bytes.hex(b"\x01\x02\x03\x04\x05", ":", 2), b"xyz".hex("-", -2))
try: b.a2b_base64(b"", True)
except TypeError as e: print(e)
import array
print(b.hexlify(array.array('H', [1, 2])))
