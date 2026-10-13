define two-lines
@echo one
-@false
echo two
endef

N = 1
define now :=
n is $(N)
endef
define later
n is $(N)
endef
N = 2

define LIST +=
c
endef
LIST = a b

override O = makefile
override define OD
from define
endef

all:
	$(two-lines)
	echo [$(now)] [$(later)] [$(LIST)] [$(O)] [$(OD)]
