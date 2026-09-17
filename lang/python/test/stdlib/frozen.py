# frozendict (PEP 814): the mapping, its hash and its methods, and a subclass.
fd = frozendict(a=1, b=2)
print(fd, repr(frozendict()), type(fd).__name__, frozendict.__module__, len(fd), fd['a'], 'a' in fd, list(fd))
print(fd == {'a': 1, 'b': 2}, {'b': 2, 'a': 1} == fd, fd == frozendict(b=2, a=1), fd != 1)
print(hash(fd) == hash(frozendict(b=2, a=1)), hash(frozendict()) == hash(frozendict()))
print(fd.get('c', 9), fd.keys(), fd.values(), fd.items(), fd.copy() is fd, dict(fd), frozendict(fd) is fd)
print(frozendict([(1, 2)]), frozendict({3: 4}, x=5), frozendict.fromkeys('ab', 0), type(frozendict.fromkeys('a')).__name__)
print(fd | {'c': 3}, {'c': 3} | fd, type({'c': 3} | fd).__name__, fd | frozendict() is fd, frozendict() | fd is fd)
x = fd
x |= {'z': 0}
print(x, fd, x is fd)
for bad in (lambda: fd.__setitem__('a', 2), lambda: fd.__delitem__('a'), lambda: fd.update({}), lambda: fd.pop('a'),
            lambda: fd.setdefault('q'), lambda: fd.clear(), lambda: hash(frozendict(a=[])), lambda: frozendict(1),
            lambda: frozendict({}, {}), lambda: fd['zz']):
    try:
        bad()
    except Exception as e:
        print(type(e).__name__, e)
try:
    fd['a'] = 5
except TypeError as e:
    print(e)
try:
    del fd['a']
except TypeError as e:
    print(e)
print(frozendict[str, int], fd.__getnewargs__(), list(reversed(fd)), sorted(dir(frozendict))[-8:])
class F(frozendict):
    pass
f = F(k=1)
print(type(f).__name__, f, f['k'], isinstance(f, frozendict), f == {'k': 1}, type(f.copy()).__name__)
print(issubclass(frozendict, dict), {fd: 1}[frozendict(a=1, b=2)], frozendict({1: frozendict()}))
import copy
print(copy.copy(fd) is fd, copy.deepcopy(frozendict(a=[1]))['a'])
match fd:
    case {'a': v}:
        print('matched', v)
print(frozendict(dict.fromkeys(range(5))).__len__(), str(frozendict(a='x')))
