.POSIX:
lib.a: lib.a(a.o) lib.a(b.o)
	echo [$@] [$%] [$?]
lib.a(b.o): hdr.h
