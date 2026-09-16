# math and cmath. The floating half is printed to fifteen digits rather than
# exactly: musl and the host's libm differ in the last place for asin and
# lgamma, and a transcendental function's last ulp is not what is being
# tested here. The exact half -- factorial, comb, gcd, isqrt -- is printed as
# it stands, because it is exact.
import math
import cmath


def near(x):
    return "%.14g" % x


print(near(math.pi), near(math.e), near(math.tau), math.inf, math.isnan(math.nan))

for name in ("sqrt", "exp", "log", "log2", "log10", "sin", "cos", "tan",
             "asin", "acos", "atan", "sinh", "cosh", "tanh", "fabs", "erf",
             "erfc", "expm1", "log1p", "gamma", "lgamma"):
    print(name, near(getattr(math, name)(0.5)))

print(near(math.atan2(1.0, 2.0)), math.hypot(3.0, 4.0), math.dist([0, 0], [3, 4]))
print(math.fmod(7.5, 2.0), math.remainder(7.5, 2.0), math.copysign(1.0, -0.0))
print(math.ldexp(1.5, 10), math.frexp(12.0), math.modf(3.75))
print(math.degrees(math.pi), near(math.radians(180.0)))
print(math.floor(-1.5), math.ceil(-1.5), math.trunc(-1.5), math.floor(7), math.ceil(7))
print(math.isfinite(1.0), math.isinf(math.inf), math.isclose(1.0, 1.0 + 1e-12))
print(math.isclose(1.0, 1.2, rel_tol=0.5), math.isclose(0.0, 1e-12, abs_tol=1e-9))
print(math.fsum([0.1] * 10), sum([0.1] * 10))
print(math.pow(2.0, 10.0), math.ulp(1.0), math.nextafter(1.0, 2.0))
print(near(math.log(8, 2)), near(math.log(1000, 10)))

print(math.factorial(20), math.factorial(25))
print(math.comb(52, 5), math.perm(52, 5), math.comb(5, 7), math.comb(0, 0))
print(math.gcd(48, 180), math.gcd(12, 18), math.gcd(-4, 6))
print(math.isqrt(0), math.isqrt(1), math.isqrt(99), math.isqrt(100))
print(math.isqrt(10 ** 20), math.isqrt(2 ** 101))
print(math.prod([1, 2, 3, 4]), math.prod([], start=7), math.prod([2.0, 3.0]))

for call in ("math.sqrt(-1)", "math.log(0)", "math.log(-1)", "math.factorial(-1)",
             "math.isqrt(-1)", "math.comb(-1, 1)", "math.exp(10000)",
             "math.fmod(1.0, 0.0)", "math.floor(math.inf)"):
    try:
        eval(call)
        print(call, "no error")
    except Exception as e:
        print(call, type(e).__name__)

print(near(cmath.pi), near(cmath.e))
print(cmath.sqrt(-1), cmath.sqrt(4), cmath.exp(0j))
print(near(cmath.phase(1j)), cmath.rect(1.0, 0.0))
print(cmath.isnan(complex(float("nan"), 0)), cmath.isinf(complex(0, float("inf"))))
print(cmath.isfinite(1 + 2j), cmath.isclose(1 + 1j, 1 + 1j))
print(abs(cmath.exp(1j * cmath.pi) + 1) < 1e-15)
print(abs(cmath.log(cmath.exp(2 + 3j)) - (2 + 3j)) < 1e-12)
print(abs(cmath.sin(1 + 1j) ** 2 + cmath.cos(1 + 1j) ** 2 - 1) < 1e-12)
print(abs(cmath.tan(1 + 1j) - cmath.sin(1 + 1j) / cmath.cos(1 + 1j)) < 1e-12)
print(abs(cmath.asin(cmath.sin(0.3 + 0.4j)) - (0.3 + 0.4j)) < 1e-12)
print(abs(cmath.atan(cmath.tan(0.3 + 0.4j)) - (0.3 + 0.4j)) < 1e-12)
print(abs(cmath.acos(cmath.cos(0.3 + 0.4j)) - (0.3 + 0.4j)) < 1e-12)
print(abs(cmath.asinh(cmath.sinh(0.3 + 0.4j)) - (0.3 + 0.4j)) < 1e-12)
print(abs(cmath.acosh(cmath.cosh(0.3 + 0.4j)) - (0.3 + 0.4j)) < 1e-12)
print(abs(cmath.atanh(cmath.tanh(0.3 + 0.4j)) - (0.3 + 0.4j)) < 1e-12)
print(abs(cmath.cosh(1 + 1j) ** 2 - cmath.sinh(1 + 1j) ** 2 - 1) < 1e-12)
print(abs(cmath.log10(100) - 2) < 1e-12)
print(abs(cmath.polar(1j)[0] - 1.0) < 1e-15, abs(cmath.polar(1j)[1] - cmath.pi / 2) < 1e-15)
