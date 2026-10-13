prog: a.c b.c a.c
	echo [$^] [$<]
x.o: hdr.h
	echo [$^] [$<] [$*]
.SUFFIXES: .o .c
.c.o:
	echo never
