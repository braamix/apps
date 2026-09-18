# ipaddress: v4 and v6 addresses, networks, interfaces, and the tables.
import ipaddress as ip

a = ip.ip_address("192.168.1.10")
print(repr(a), int(a), a.packed, a.version, a.is_private, a.is_global, a.reverse_pointer)
print(a + 5, a - 10, a > ip.IPv4Address("10.0.0.1"), str(ip.IPv4Address(3232235777)))
n = ip.ip_network("192.168.0.0/22")
print(n, n.netmask, n.hostmask, n.broadcast_address, n.num_addresses, n.prefixlen, n.with_netmask)
print(list(n.subnets(prefixlen_diff=2)), n.supernet(), ip.ip_address("192.168.3.4") in n)
print(list(ip.ip_network("10.0.0.0/30").hosts()), ip.ip_network("10.0.0.0/8").overlaps(n))
print(list(ip.summarize_address_range(ip.IPv4Address("10.0.0.0"), ip.IPv4Address("10.0.0.10"))))
print(list(ip.ip_network("10.0.0.0/24").address_exclude(ip.ip_network("10.0.0.0/26"))))
i = ip.ip_interface("10.1.2.3/24")
print(repr(i), i.network, i.ip, i.with_prefixlen, i.with_hostmask)
b = ip.ip_address("2001:db8::8a2e:370:7334")
print(repr(b), b.exploded, b.compressed, int(b), b.is_global, b.is_private, b.version)
print(b.reverse_pointer)
print(ip.ip_address("::ffff:192.0.2.1").ipv4_mapped, ip.ip_address("fe80::1%eth0").scope_id)
print(ip.ip_address("::1").is_loopback, ip.ip_address("ff02::1").is_multicast,
      ip.ip_address("fe80::1").is_link_local, ip.ip_address("fc00::1").is_private)
v6 = ip.ip_network("2001:db8::/126")
print(v6, v6.num_addresses, list(v6), v6.supernet(new_prefix=64))
print(ip.IPv6Network("2001:db8::/32").is_global, ip.ip_address("2002:c000:0204::1").sixtofour)
print(sorted([ip.ip_address("10.0.0.2"), ip.ip_address("10.0.0.1")]), ip.v4_int_to_packed(1))
print(hash(ip.ip_address("1.2.3.4")) == hash(ip.ip_address("1.2.3.4")), ip.get_mixed_type_key(a))
for bad in ["1.2.3", "256.1.1.1", "::g", "1.2.3.4/33", "10.0.0.1/24"]:
    try:
        ip.ip_network(bad)
    except ValueError as e:
        print(type(e).__name__, e)
print(ip.ip_network("10.0.0.1/24", strict=False), format(a, "b")[:12], format(a, "#x"), f"{b:_X}")
