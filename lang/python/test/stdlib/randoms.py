# random over _random: the Mersenne Twister, seeded, gives CPython's numbers.
import random

random.seed(12345)
print(random.random(), random.getrandbits(70), random.randrange(1000))
print(random.randint(-5, 5), random.choice("abcdef"), random.uniform(1, 2))
x = list(range(10))
random.shuffle(x)
print(x, random.sample(range(100), 5), random.choices("abc", k=6))
print(random.choices("abc", weights=[1, 0, 5], k=5), random.choices(range(3), cum_weights=[0, 1, 2], k=3))
# The variates go through log and exp, which no two libms round alike.
def r12(*xs):
    return [round(x, 12) for x in xs]

print(r12(random.gauss(0, 1), random.normalvariate(10, 2), random.expovariate(3)))
print(r12(random.triangular(0, 10, 3), random.betavariate(2, 3), random.gammavariate(2, 1)))
print(r12(random.lognormvariate(0, 1), random.vonmisesvariate(1, 2), random.paretovariate(3)))
print(r12(random.weibullvariate(1, 2)), random.binomialvariate(20, 0.3), random.randbytes(6))

# A str or bytes seed goes through hashlib's sha512, which is not here yet.
r = random.Random(2**100 + 7)
print(r.random(), r.randrange(0, 10**30, 7))
r.seed(-3)
print(r.random())
st = r.getstate()
a = [r.random() for _ in range(3)]
r.setstate(st)
print(a == [r.random() for _ in range(3)], st[0], len(st[1]))
r.seed(1, version=1)
print(r.random())
print(random.Random(5).sample(["x", "y", "z"], counts=[1, 2, 3], k=4))

for bad in (lambda: random.randrange(0), lambda: random.choice([]),
            lambda: random.sample([1, 2], 3), lambda: random.randint(1, 0),
            lambda: random.randrange(1, 10, 0), lambda: random.getrandbits(-1)):
    try:
        bad()
    except (ValueError, IndexError) as e:
        print(type(e).__name__, e)

class Mine(random.Random):
    def random(self):
        return 0.5

print(Mine(0).random(), Mine(0).choice([1, 2, 3]), isinstance(random.SystemRandom().random(), float))
print(0 <= random.SystemRandom().randrange(10) < 10, random.SystemRandom().getrandbits(5) < 32)
