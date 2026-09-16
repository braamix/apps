# _typing: TypeVar, ParamSpec, TypeVarTuple, TypeAliasType, Generic and NoDefault.
from _typing import TypeVar, ParamSpec, TypeVarTuple, TypeAliasType, Generic, NoDefault, Union, _idfunc, ParamSpecArgs, ParamSpecKwargs
T = TypeVar('T')
print(repr(T), T.__name__, T.__bound__, T.__constraints__, T.__covariant__, T.__contravariant__, T.__infer_variance__, T.__default__, T.has_default(), T.__module__)
tv = TypeVar("TV", int, str, covariant=True)
print(repr(tv), tv.__constraints__)
tv2 = TypeVar("TV2", bound=int, contravariant=True, default=str)
print(repr(tv2), tv2.__bound__, tv2.__default__, tv2.has_default())
print(repr(TypeVar("I", infer_variance=True)))
for bad in [dict(covariant=True, contravariant=True), dict(covariant=True, infer_variance=True)]:
    try: TypeVar("X", **bad)
    except Exception as e: print(type(e).__name__, e)
try: TypeVar("X", int)
except Exception as e: print(type(e).__name__, e)
try: TypeVar("X", int, str, bound=int)
except Exception as e: print(type(e).__name__, e)
try:
    class D(T): pass
except Exception as e: print(type(e).__name__, e)
try:
    class E(Generic): pass
except Exception as e: print(type(e).__name__, e)
try: Generic[int]
except Exception as e: print(type(e).__name__, e)
try: Generic[T, T]
except Exception as e: print(type(e).__name__, e)
P = ParamSpec("P")
print(repr(P), P.args, P.kwargs, P.args == P.args, P.args.__origin__ is P, type(P.args) is ParamSpecArgs, type(P.kwargs) is ParamSpecKwargs)
Ts = TypeVarTuple("Ts")
print(repr(Ts), [*Ts], Ts.__default__, TypeVarTuple("Z", default=int).has_default())
print(NoDefault, repr(NoDefault), type(NoDefault).__name__)
print(_idfunc(5), Union[int, str], Union is type(int | str))
class G(Generic[T]): pass
print(G.__parameters__, G.__mro__[1:], G.__orig_bases__, G[int])
try: G[int, str]
except Exception as e: print(type(e).__name__, e)
A = TypeAliasType("A", list[T], type_params=(T,))
print(repr(A), A.__value__, A[int], A.__type_params__, A.__parameters__, A.__module__)
try:
    TypeAliasType("B", int)[int]
except Exception as e: print(type(e).__name__, e)
try: A.__value__ = 1
except AttributeError as e: print("AE", e)
print(T | None, int | T, T.__typing_subst__(int), list[T][str], (T | None)[int])
print(type(Generic()).__name__)
class K(G[T]): pass
print(K.__parameters__, K.__orig_bases__)
U = TypeVar("U", default=int)
class H(Generic[T, U]): pass
print(H[str], H.__parameters__)
