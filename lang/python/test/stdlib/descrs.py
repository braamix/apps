# __get__, __set__ and __delete__ on the built-in descriptors.
def f(): pass
print(hasattr(f, "__get__"), hasattr(property(f), "__get__"), hasattr(classmethod(f), "__get__"), hasattr(staticmethod(f), "__set__"), hasattr(property(f), "__delete__"), hasattr(len, "__get__"), hasattr(1, "__get__"))
print(type(f.__get__(None, int)).__name__)
class C:
    @property
    def p(s): return 1
    __slots__=('m',)
c=C()
P=C.__dict__['p']; M=C.__dict__['m']
for f in (lambda: P.__set__(c, 1), lambda: P.__delete__(c), lambda: P.__get__(None, None), lambda: P.__get__(), lambda: M.__get__(c), lambda: M.__set__(c, 5) or M.__get__(c, C), lambda: M.__delete__(c) or c.m):
    try: print(f())
    except Exception as e: print(type(e).__name__, e)
print(P.__get__(c), P.__get__(None, C) is P, classmethod(len).__get__(None, int).__self__, staticmethod(len).__get__(1) is len)
