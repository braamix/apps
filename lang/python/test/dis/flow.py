n = 0
while n < 10:
    n = n + 1
    if n == 3:
        continue
    if n == 7:
        break
else:
    n = -1

for i in range(4):
    for j in range(4):
        if i == j:
            break
    else:
        n = n + i

if n > 0:
    x = 1
elif n < 0:
    x = 2
else:
    x = 3
