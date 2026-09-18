# urllib.parse: split, join, quote, and the query string.
from urllib import parse as up

u = up.urlparse("https://user:pw@Example.COM:8080/a/b;p?q=1&r=2#frag")
print(u)
print(u.scheme, u.netloc, u.hostname, u.port, u.username, u.password, u.geturl())
print(up.urlsplit("http://[::1]:80/x?y#z"), up.urlsplit("//host/p").netloc)
print(up.urlunparse(("http", "h", "/p", "", "a=1", "f")), up.urlunsplit(u[:2] + ("/z", "k", "")))
for rel in ["g", "./g", "../g", "/g", "//g", "?y", "#s", "../../../g", "g;x?y#s", ""]:
    print(repr(rel), up.urljoin("http://a/b/c/d;p?q", rel))
print(up.urldefrag("http://a/b#c"), up.urldefrag("http://a/b"))
print(up.quote("a b/c&d=é"), up.quote("a b/c", safe=""), up.quote_plus("a b+c"))
print(up.unquote("a%20b%2Fc%C3%A9"), up.unquote_plus("a+b%2B"), up.unquote_to_bytes("%ff%41"))
print(up.quote(b"\xff\x00"), up.quote_from_bytes(b"a b"))
print(up.urlencode({"a": 1, "b": "x y", "c": ["p", "q"]}, doseq=True), up.urlencode([("k", "v&w")]))
print(up.parse_qs("a=1&a=2&b=&c=%20x", keep_blank_values=True), up.parse_qsl("x=1;y=2", separator=";"))
print(up.parse_qsl("a=1&b"), up.parse_qs("a=1&b=2", max_num_fields=5))
b = up.urlparse(b"http://h/p?q")
print(b, b.decode().geturl())
print(up.urlsplit("mailto:someone@example.com"), up.urlsplit("file:///tmp/x"))
try:
    up.urlsplit("http://[::1/").hostname
except ValueError as e:
    print("ValueError", e)
try:
    up.urlparse("http://h:bad/").port
except ValueError as e:
    print("ValueError", e)
print(up.uses_relative[:4], up.unwrap("<URL:http://a/b>"))
