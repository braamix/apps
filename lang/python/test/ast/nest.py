x = [[y for y in row] for row in grid]
z = {k: [v for v in vs] for k, vs in d.items()}
w = sum(a * b for a, b in pairs)
q = [a for a in b for c in d if c if a]
def outer():
    def inner():
        class Deep:
            def m(self):
                return lambda: [i for i in range(3)]
        return Deep
    return inner
