.POSIX:
W = world
A != echo hello $(W); echo second line
C :::= cost $$5 $(W)
C += more $(W)
D ::= $(W)
W = later
all:
	echo '[$(A)] [$(C)] [$(D)] [$(X)]'
