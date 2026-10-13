X = $(or ,  x  ,y)|$(and a, b )|$(and a,,c)|$(if  ,a,b)|$(if x , a , b )|$(if ,a)
S = $(subst ,x,abc)|$(subst a,A,banana)|$(sort  b a  b c)|$(dir a/b c)|$(notdir a/b c)
T = $(suffix a.c b d/e.f.g h.d/i)|$(basename a.c d/e.f.g h.d/i)|$(join a b c,1 2)
U = $(wordlist 2,3,a b c d)|$(word 2, a b c)|$(word 9,a)|$(words a b  c)|$(words )
V = $(patsubst %.c,%.o,a.c b.h)|$(patsubst a,b,a ab)|$(filter %.c x,a.c x y)
W = $(filter-out %.c,a.c b.h)|$(findstring a,cat)|$(findstring z,cat)|$(strip   a   b  )
Y = $(firstword a b)|$(lastword a b)|$(addsuffix .c,a b)|$(addprefix d/,a b)
SRCS = one.c two.c
Z = $(foreach f,$(SRCS),[$(f:.c=)])|$(f)|$(patsubst %.c,%.o,$(filter %.c,$(SRCS)))
N = $(subst $(comma),;,a,b)
comma := ,
all:
	echo "$(X)"
	echo "$(S)"
	echo "$(T)"
	echo "$(U)"
	echo "$(V)"
	echo "$(W)"
	echo "$(Y)"
	echo "$(Z)"
	echo "$(N)"
