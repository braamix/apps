SRCS = a.c b.c
-include $(SRCS:.c=.d)
all: $(SRCS:.c=.o)
%.o: %.c
	echo cc $<
%.d: %.c
	echo "$*.o: hdr.h" > $@
