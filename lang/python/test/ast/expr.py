a = 1 + 2 * 3 - 4 / 5
b = 2 ** 3 ** 2
c = -2 ** 2
d = not a and b or c
e = a < b <= c != d
f = a if b else c
g = (a, b, c)
h = [a, b]
i = {1: 'x', 2: 'y'}
j = {a, b}
k = a.b.c(d, e=1, *f, **g)
m = a[1], a[1:2], a[::2], a[1:2:3]
n = lambda x, y=1, *z, w, **v: x
o = [x * 2 for x in range(3) if x if x > 1]
p = {k: v for k, v in items}
q = (x for x in y)
r = {x for x in y}
s = ~a | b & c ^ d << e >> f
t = a is not b
u = a not in b
v = (n2 := 5)
