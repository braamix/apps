# uuid: parsing, the fields, the name-based versions, and what random ones share.
import uuid

u = uuid.UUID("12345678-1234-5678-1234-567812345678")
print(repr(u), str(u), u.hex, u.int, u.urn, u.version, u.variant)
print(u.fields, u.time_low, u.time_mid, u.time_hi_version, u.clock_seq, u.node, u.bytes_le.hex())
print(uuid.UUID("{12345678-1234-5678-1234-567812345678}") == u, uuid.UUID(bytes=u.bytes) == u,
      uuid.UUID(int=u.int) == u, uuid.UUID(fields=u.fields) == u, uuid.UUID(bytes_le=u.bytes_le) == u)
print(uuid.uuid3(uuid.NAMESPACE_DNS, "python.org"), uuid.uuid5(uuid.NAMESPACE_URL, "http://x/"))
print(uuid.uuid8(1, 2, 3), uuid.NIL, uuid.MAX, uuid.NAMESPACE_OID, uuid.NAMESPACE_X500)
r = uuid.uuid4()
print(r.version, r.variant, len(str(r)), r != uuid.uuid4())
print(uuid.uuid1().version, uuid.uuid6().version, uuid.uuid7().version, isinstance(uuid.getnode(), int))
print(sorted([uuid.UUID(int=2), uuid.UUID(int=1)]), uuid.UUID(int=5) < uuid.UUID(int=6), uuid.SafeUUID.unknown)
for bad in ["xyz", "12345678-1234-5678-1234-56781234567"]:
    try:
        uuid.UUID(bad)
    except ValueError as e:
        print("ValueError", e)
try:
    u.hex = "x"
except TypeError as e:
    print("TypeError", e)
