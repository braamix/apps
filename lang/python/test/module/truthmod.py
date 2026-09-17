# Truth written in Python: __bool__ and __len__ as the jumps, `not`, bool(),
# any() and all() ask them.
class C:
    def __bool__(s): return False
    def __len__(s): return 0
if C(): print("yes")
else: print("no")
print(not C(), type(C() and 1).__name__, bool(C()))
class L:
    def __len__(s): return 2
class Z:
    def __len__(s): return 0
class B:
    def __bool__(s): return 1
print([1 if x else 0 for x in (L(), Z())], type(L() or 5).__name__, Z() or 5, not Z(), not L())
while Z():
    print("never")
try:
    if B(): pass
except TypeError as e:
    print(e)
x = 0
assert L(), "no"
print(all([L(), L()]), any([Z()]), [z for z in (L(), Z()) if z].__len__())
