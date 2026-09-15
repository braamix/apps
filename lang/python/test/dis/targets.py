a = [0, 1, 2]
a[0] = 1
a[0:2] = a
a[::2] = a
a[0] += 1
o = object()
o.x = 1
o.x += 1
first, *rest = a
*most, last = a
x = y = z = 0
del a[0], o.x, x
q = (w := 1 + 1)
t = 1 < w <= 3 != 4
u = 1 < w
v = t and u or not t
s = a[1:2:1]
print(*a, **{})
b = [*a, 1, *a]
d = {**{}, 'k': 1}
