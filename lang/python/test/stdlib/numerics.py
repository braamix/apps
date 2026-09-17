# decimal, datetime and statistics: three pure-Python modules over the native floor.
import decimal, datetime, statistics, fractions
from decimal import Decimal as D, getcontext, localcontext
print(D(1) / D(7), D("1e-30").sqrt(), D(2) ** D("0.5"), D("123.456").quantize(D("0.01")), D(10).ln(), D(3).exp())
with localcontext() as c:
    c.prec = 50
    print(D(1) / D(3), c)
print(getcontext().prec, D("NaN"), D("-0"), D(0.1), D.from_float(1.5), round(D("2.675"), 2), f"{D('1234.5'):,.2f}")
try: D(1) / D(0)
except decimal.DivisionByZero as e: print("div0", type(e).__mro__[:3])
print(fractions.Fraction(D("0.25")), D(5) % D(3), divmod(D(-7), D(2)), hash(D("1.5")) == hash(1.5))
d = datetime.date(2024, 2, 29)
print(d, d + datetime.timedelta(days=366), d.isoformat(), d.weekday(), tuple(d.isocalendar()), d.strftime("%A %d %B %Y %j"))
t = datetime.datetime(2020, 5, 17, 13, 45, 30, 123456, tzinfo=datetime.timezone.utc)
print(t, t.isoformat(), t.timestamp(), t.astimezone(datetime.timezone(datetime.timedelta(hours=-5))), repr(t))
print(datetime.datetime.fromisoformat("2011-11-04T00:05:23+04:00"), datetime.timedelta(days=1, seconds=5, microseconds=-1), datetime.datetime.strptime("2021-03-04 05:06", "%Y-%m-%d %H:%M"))
print(datetime.datetime.fromtimestamp(0, datetime.UTC), datetime.date.fromordinal(730000), datetime.time(1, 2, 3).replace(hour=4))
print(statistics.mean([1, 2, 3, 4]), statistics.median([3, 1, 2]), statistics.stdev([1.5, 2.5, 2.5, 2.75, 3.25, 4.75]), statistics.mode("aabbbc"))
print(statistics.fmean([1, 2, 3]), statistics.geometric_mean([54, 24, 36]), statistics.quantiles(range(1, 11), n=4), statistics.NormalDist(100, 15).cdf(130))
print(statistics.linear_regression([1, 2, 3, 4, 5], [1, 2, 3, 4, 5.5]), statistics.correlation([1, 2, 3], [2, 4, 7]), statistics.mean([D("1.1"), D("2.2")]))
