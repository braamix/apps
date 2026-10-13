A = one
A += two
B := $(A)
A += three
C ?= first
C ?= second
F := [$(E)]
D = $(E)
E = late
G = x
G += $(E)
H ::= h$$
S := s
S += $(E)
_U = under
all:
	echo [$(A)] [$(B)] [$(C)] [$(D)] [$(F)] [$(G)] [$(H)] [$(S)] [$(_U)]
