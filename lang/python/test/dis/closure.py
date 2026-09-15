counter = 0

def outer(start):
    total = start

    def inner(step):
        nonlocal total
        total = total + step
        return total

    def deeper():
        def deepest():
            return total
        return deepest
    return inner, deeper

def uses_global():
    global counter
    counter = counter + 1
    return counter
