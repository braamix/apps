.SUFFIXES: .o .c
.c.o:
	echo suffix $<
%.o: %.c
	echo pattern $<
all: a.o b.o
b.o: b.c
	echo explicit $<
