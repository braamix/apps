OBJS = a.o b.o
all: $(OBJS) x.out
$(OBJS): %.o: %.c | hdr.h
	echo static $@ from $< stem $* [$|]
x.out: %.out: %.c
	echo $* $<
