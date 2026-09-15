xs = [i * 2 for i in range(5) if i]
ys = {i for i in xs}
zs = {i: i * i for i in xs if i > 2 if i < 9}
gs = (i for i in xs)
nested = [(i, j) for i in xs for j in ys if i != j]
