# time without the local zone: strftime's directives, struct_time, the checks on each argument.
import time
t = (2024, 2, 29, 13, 5, 9, 3, 60, 0)
for f in ['%c', '%x', '%X', '%D', '%F', '%T', '%R', '%r', '%C', '%g', '%G', '%V', '%u', '%U', '%W', '%k', '%l', '%e', '%P', '%Q', '%Ey', '%Od', '%h', '%n', '%t', '%%', 'abc%', '%', '%5Y', '%-d', '%_d', '%0e', '%v', '%i', '%N', '%L', '%E', '%O', 'é%Y€']:
    try: print(repr(f), repr(time.strftime(f, t)))
    except Exception as e: print(repr(f), type(e).__name__, e)
print(repr(time.strftime('%Y %y %C %G %g', (-5, 1, 1, 0, 0, 0, 0, 1, 0))), repr(time.strftime('%Y %y', (12345, 1, 1, 0, 0, 0, 0, 1, 0))), repr(time.strftime('%Y', (99, 1, 1, 0, 0, 0, 0, 1, 0))))
print(time.gmtime(0), time.gmtime(1.5).tm_zone, time.gmtime(0).tm_gmtoff)
for a in [float('nan'), 1e300, 2**70, 'x', -1.5]:
    try: print(time.gmtime(a))
    except Exception as e: print(type(e).__name__, e)
for a in [(2024,), [2024,1,1,0,0,0,0,1,0], (2024,13,1,0,0,0,0,1,0), (2024,1,1,0,0,0,-2,1,0), (2024,1,1,0,0,0,0,1,0), (2024,0,0,0,0,0,0,0,0), (2024.0,1,1,0,0,0,0,1,0)]:
    try: print(time.asctime(a))
    except Exception as e: print(type(e).__name__, e)
print(time.struct_time((1,2,3,4,5,6,7,8,9)), time.struct_time((1,2,3,4,5,6,7,8,9,'X',3)).tm_zone)
try: time.struct_time((1,2))
except TypeError as e: print(e)
try: time.struct_time((1,)*12)
except TypeError as e: print(e)
print(time.strptime('2024-03-04', '%Y-%m-%d'), time.strptime('Thu Feb 29 13:05:09 2024'))
for w in [(2021, 1, 1), (2020, 12, 31), (2027, 1, 3), (2026, 12, 28), (2004, 12, 27), (2005, 1, 2), (2010, 1, 3)]:
    import calendar
    st = time.gmtime(calendar.timegm(w + (0, 0, 0)))
    print(time.strftime('%G-%V-%u %g %U %W %j %a', st), st)
try: time.sleep(float('nan'))
except Exception as e: print(type(e).__name__, e)
try: time.sleep('1')
except Exception as e: print(type(e).__name__, e)
try: time.get_clock_info('x')
except Exception as e: print(type(e).__name__, e)
try: time.clock_gettime(99)
except Exception as e: print(type(e).__name__, e)
print(time._STRUCT_TM_ITEMS, type(time.clock_gettime(time.CLOCK_MONOTONIC)), time.gmtime(10**10), time.strftime('%Y', time.gmtime(-10**11)))
print(calendar.month(2024, 2), calendar.isleap(2000), calendar.weekday(2024, 2, 29), calendar.monthrange(2023, 2))
