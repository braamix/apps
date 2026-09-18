# quopri: quoted-printable both ways, in and out of headers.
import io
import quopri

src = b"caf\xc3\xa9 = ok\ttab \nlong " + b"x" * 80 + b"\nend  \n"
enc = quopri.encodestring(src)
print(enc)
print(quopri.decodestring(enc) == src, quopri.encodestring(b"a b", quotetabs=True))
print(quopri.encodestring(b"h_d r", header=True), quopri.decodestring(b"h_d=20r", header=True))
print(quopri.decodestring(b"soft=\nbreak =3D =41=4"))
i, o = io.BytesIO(b"line one=\n two\n"), io.BytesIO()
quopri.decode(i, o)
print(o.getvalue())
i, o = io.BytesIO(b"\xff\xfe\n"), io.BytesIO()
quopri.encode(i, o, quotetabs=False)
print(o.getvalue(), quopri.needsquoting(b"=", False, False), quopri.quote(b"="))
