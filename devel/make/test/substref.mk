SRCS = a.c   b.c hdr.h  c.c
OBJS = $(SRCS:.c=.o)
PAT = $(SRCS:%.c=obj/%.o)
O = .o
all: $(SRCS:.c=.o)
	echo '[$(OBJS)] [$(PAT)] [$(SRCS:.c=$(O))] [${SRCS:%=<%>}] [$(SRCS:.c=)]'
a.o b.o c.o:
	echo $@
