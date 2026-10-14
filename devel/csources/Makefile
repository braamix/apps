# vcc, the C compiler, built on Braam by the installed one.
#
#   make            the compiler, into obj/
#   make check      obj/ against the installed cc package, byte for byte
#   make clean      obj/ removed
#
# obj/ is laid out as the cc package is, so obj/bin/cc runs from where it is:
# its passes in obj/libexec/, its runtime and headers in obj/share/vcc/.

# The installed cc package, which obj/ is checked against.
CCDIR := $(firstword $(wildcard /pkg/store/cc-*))
CCLIB := $(CCDIR)/share/vcc/wasm32-braam/lib
INC := $(CCDIR)/share/vcc/wasm32-braam/include

# The compiler's parts, each a directory of its own.
LIBUTIL = libutil/c_escape.c libutil/string_map.c libutil/xalloc.c libutil/wio.c \
          libutil/float128.c libutil/srcloc.c libutil/readfile.c
AST = ast/ast_alloc.c ast/ast_print.c ast/ast_free.c ast/ast_clone.c ast/ast_yaml.c \
      ast/ast_export.c ast/ast_import.c ast/ast_compare.c ast/type_to_c.c
SCANNER = scanner/scanner.c
PARSER = parser/parser.c parser/expr.c parser/decl.c parser/stmt.c parser/nametab.c
TAC = tac/tac_alloc.c tac/tac_print.c tac/tac_free.c tac/tac_compare.c tac/tac_yaml.c \
      tac/tac_import.c tac/tac_export.c tac/tac_walk.c tac/tac_verify.c tac/tac_abi.c
SEMANTIC = semantic/symtab.c semantic/symtab_print.c semantic/structtab.c \
           semantic/structtab_print.c semantic/typetab.c semantic/typetab_print.c \
           semantic/typecheck.c semantic/expressions.c semantic/init_normalize.c \
           semantic/initializers.c semantic/statements.c semantic/declarations.c \
           semantic/type_utils.c semantic/const_convert.c semantic/label_loops.c \
           semantic/coroutines.c semantic/defer.c semantic/resolve_labels.c \
           semantic/target.c semantic/tac_type.c
OPTIMIZE = optimize/optimize.c optimize/const_fold.c optimize/cfg.c optimize/unreachable.c \
           optimize/alias.c optimize/dataflow.c optimize/copy_prop.c optimize/cse.c \
           optimize/ivsr.c optimize/dead_store.c optimize/liveness.c
TRANSLATOR = translator/translate.c translator/expr.c translator/stmt.c translator/coro.c
BACKEND = backend/common/driver.c backend/common/flow.c
WASM = backend/wasm/wasm_ir.c backend/wasm/emit.c backend/wasm/frame.c \
       backend/wasm/instr.c backend/wasm/data.c backend/wasm/structure.c \
       backend/wasm/call.c backend/wasm/peephole.c backend/wasm/locals.c \
       backend/wasm/codegen.c
CPP = cpp/cpp.c cpp/buffer.c cpp/scan.c cpp/direct.c cpp/macro.c cpp/diag.c \
      cpp/parser.c cpp/yylex.c cpp/files.c cpp/deps.c

# The five programs, each its main and the parts it links, in link order.
VCC_SRCS = cc/cc.c $(LIBUTIL)
VCPP_SRCS = $(CPP) $(LIBUTIL)
VPARSE_SRCS = parser/main.c $(PARSER) $(SCANNER) $(AST) $(LIBUTIL)
VLOWER_SRCS = translator/main.c $(TRANSLATOR) $(OPTIMIZE) $(SEMANTIC) $(TAC) $(AST) $(LIBUTIL)
VGENWASM_SRCS = backend/wasm/main.c $(WASM) $(BACKEND) $(TAC) $(LIBUTIL)

# Every part's headers are found by name; defer and yield are the compiler's
# own identifiers, which <coro.h> would otherwise take.
CFLAGS = -D_CORO_NO_DEFER_YIELD -Ilibutil -Iast -Iscanner -Iparser -Itac -Isemantic \
         -Ioptimize -Itranslator -Ibackend/common -Ibackend/wasm

# The runtime. libc/common is plain C for any wasm32, compiled with its own
# headers; libc/wasm32/braam is the half that knows Braam.
LIBC_COMMON = atoi bsearch co ctype float128 getopt_long gmtime memccpy memchr memcmp \
              qsort snprintf sprintf strcat strchr strcmp strcpy strcspn strdup \
              strftime strlen strncat strncmp strncpy strndup strnlen strpbrk strrchr strspn \
              strstr strtok strtok_r strtol vsnprintf vsprintf
LIBC_BRAAM = rt sys stdio malloc strerror strtod taskbytes
WASM32_INCLUDE = -nostdinc -Ilibc/wasm32/include -Ilibc/ilp32/include -Ilibc/common/include

LIBC_OBJS = obj/libc/memory.o $(LIBC_COMMON:%=obj/libc/%.o) $(LIBC_BRAAM:%=obj/libc/%.o) \
            obj/libc/doprnt.o

# The math library: wasm's one-instruction functions, then musl's C in math/,
# in the order the host build globs it. libc/wasm32/braam/musl/math.h comes
# first on the path; sincos.c and jn.c each need a word more.
MATH_SRCS = $(sort $(wildcard math/musl/*.c math/cvt/*.c))
MATH_OBJS = obj/libc/math.o $(patsubst math/%.c,obj/libc/math/%.o,$(MATH_SRCS))
MATH_FLAGS = -Ilibc/wasm32/braam/musl -Dhidden=

# The installation, as the cc package lays it out.
SHARE = obj/share/vcc/wasm32-braam
PROGRAMS = obj/bin/cc obj/libexec/vcpp obj/libexec/vparse obj/libexec/vlower \
           obj/libexec/vgenwasm
RUNTIME = $(SHARE)/lib/crt0.o $(SHARE)/lib/libc.a
INCLUDE = $(SHARE)/include/braam.h

LDFLAGS = --no-entry --import-memory --stack-first --gc-sections

obj = $(patsubst %.c,obj/%.o,$(1))

all: $(PROGRAMS) $(RUNTIME) $(INCLUDE)

# The compiler's sources, through the installed driver.
obj/%.o: %.c
	@mkdir -p $(@D)
	cc $(CFLAGS) -MMD -c -o $@ $<

$(LIBC_COMMON:%=obj/libc/%.o): obj/libc/%.o: libc/common/%.c
	@mkdir -p $(@D)
	cc $(WASM32_INCLUDE) -MMD -c -o $@ $<

$(LIBC_BRAAM:%=obj/libc/%.o): obj/libc/%.o: libc/wasm32/braam/%.c
	@mkdir -p $(@D)
	cc -MMD -c -o $@ $<

# The printf engine, which on Braam hands its floats to the math library.
obj/libc/doprnt.o: libc/common/doprnt.c
	@mkdir -p $(@D)
	cc -MMD -c -o $@ libc/common/doprnt.c

obj/libc/math.o: libc/wasm32/math.s
	@mkdir -p $(@D)
	as -o $@ libc/wasm32/math.s

obj/libc/math/%.o: math/%.c
	@mkdir -p $(@D)
	cc $(MATH_FLAGS) -MMD -c -o $@ $<

obj/libc/math/musl/sincos.o obj/libc/math/musl/sincosf.o: MATH_FLAGS += -DMUSL_SINCOS
obj/libc/math/musl/jn.o obj/libc/math/musl/jnf.o: MATH_FLAGS += -DMUSL_JN

obj/libc/memory.o: libc/wasm32/memory.s
	@mkdir -p $(@D)
	as -o $@ libc/wasm32/memory.s

# Braam's exports: _start, _resume and the rest.
$(SHARE)/lib/crt0.o: libc/wasm32/braam/exports.s
	@mkdir -p $(@D)
	as -o $@ libc/wasm32/braam/exports.s

# libc.a is our objects, then the math library's.
$(SHARE)/lib/libc.a: $(LIBC_OBJS) $(MATH_OBJS)
	@mkdir -p $(@D)
	test ! -f $@ || rm $@
	ar qc $@ $(LIBC_OBJS)
	ar qcs $@ $(MATH_OBJS)

# The headers, the first of a name winning: Braam's, then wasm32's, ILP32's
# and the common ones. Copied last to first, so the first is the one left.
$(INCLUDE): $(wildcard libc/*/include/*.h libc/wasm32/braam/include/*.h \
                       libc/wasm32/braam/include/sys/*.h)
	@mkdir -p $(@D)/sys
	cp libc/common/include/*.h $(@D)
	cp libc/ilp32/include/*.h $(@D)
	cp libc/wasm32/include/*.h $(@D)
	cp libc/wasm32/braam/include/*.h $(@D)
	cp libc/wasm32/braam/include/sys/*.h $(@D)/sys

# The links: the stack, the initial memory in bytes and pages, and 1600 pages,
# 100 MB, at most. ld names the module after its output, so each program is
# linked under the name the cc package's was, and copied into place.
obj/vcc.wasm: $(call obj,$(VCC_SRCS)) $(RUNTIME)
	ld $(LDFLAGS) -z stack-size=131072 --initial-memory=262144 --braam-pages=4,1600 \
	    -o $@ $(SHARE)/lib/crt0.o $(call obj,$(VCC_SRCS)) $(SHARE)/lib/libc.a

obj/cpp.wasm: $(call obj,$(VCPP_SRCS)) $(RUNTIME)
	ld $(LDFLAGS) -z stack-size=1048576 --initial-memory=3145728 --braam-pages=48,1600 \
	    -o $@ $(SHARE)/lib/crt0.o $(call obj,$(VCPP_SRCS)) $(SHARE)/lib/libc.a

obj/parse.wasm: $(call obj,$(VPARSE_SRCS)) $(RUNTIME)
	ld $(LDFLAGS) -z stack-size=1048576 --initial-memory=2097152 --braam-pages=32,1600 \
	    -o $@ $(SHARE)/lib/crt0.o $(call obj,$(VPARSE_SRCS)) $(SHARE)/lib/libc.a

obj/lower.wasm: $(call obj,$(VLOWER_SRCS)) $(RUNTIME)
	ld $(LDFLAGS) -z stack-size=1048576 --initial-memory=2097152 --braam-pages=32,1600 \
	    -o $@ $(SHARE)/lib/crt0.o $(call obj,$(VLOWER_SRCS)) $(SHARE)/lib/libc.a

obj/genwasm.wasm: $(call obj,$(VGENWASM_SRCS)) $(RUNTIME)
	ld $(LDFLAGS) -z stack-size=1048576 --initial-memory=2097152 --braam-pages=32,1600 \
	    -o $@ $(SHARE)/lib/crt0.o $(call obj,$(VGENWASM_SRCS)) $(SHARE)/lib/libc.a

obj/bin/cc: obj/vcc.wasm
	@mkdir -p $(@D)
	cp obj/vcc.wasm $@

obj/libexec/v%: obj/%.wasm
	@mkdir -p $(@D)
	cp $< $@

# What was built here against what the cc package was built as on the host.
check: all
	cmp obj/bin/cc $(CCDIR)/bin/cc
	cmp obj/libexec/vcpp $(CCDIR)/libexec/vcpp
	cmp obj/libexec/vparse $(CCDIR)/libexec/vparse
	cmp obj/libexec/vlower $(CCDIR)/libexec/vlower
	cmp obj/libexec/vgenwasm $(CCDIR)/libexec/vgenwasm
	cmp $(SHARE)/lib/crt0.o $(CCLIB)/crt0.o
	cmp $(SHARE)/lib/libc.a $(CCLIB)/libc.a
	for d in . sys; do for h in `ls $(INC)/$$d`; do test -d $(INC)/$$d/$$h || \
	    cmp $(SHARE)/include/$$d/$$h $(INC)/$$d/$$h || exit 1; done; done
	@echo "obj/ is the cc package, byte for byte."

clean:
	test ! -d obj || rm -r obj

.PHONY: all check clean
.DELETE_ON_ERROR:

# What -MMD wrote, once there is any.
-include $(wildcard obj/*/*.d obj/*/*/*.d obj/*/*/*/*.d)
