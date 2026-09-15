try:
    x = 1
except ValueError:
    x = 2
except (TypeError, KeyError) as e:
    x = e
except:
    x = 4
else:
    x = 5
finally:
    x = 6

try:
    raise ValueError('bad')
except ValueError as err:
    raise RuntimeError('worse') from err

with open('a') as f, open('b'):
    pass

assert x, 'must hold'

for i in range(3):
    try:
        if i:
            break
        continue
    finally:
        x = i
