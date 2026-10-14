# The wasm32 half of devel/cc, under the tree's own CMakeLists: the runtime,
# crt0.o and libc.a, and programs written in C with coroutines, all compiled
# by the host compiler in VCC_HOST and assembled and linked by the SDK's
# tools. The top Makefile builds and installs the host compiler first.
#
#   vcc_add_program(NAME hello SOURCES hello.c [FLAGS -I... -D...] [STACK bytes]
#                   [PAGES initial] [TASK_BYTES bytes])
#
# TASK_BYTES is the root task's arena, the frames of everything main awaits
# (libc/wasm32/braam/taskbytes.c); 64 KiB by default.
#
# defines target bin_<name> producing <name>.wasm, stamped as
# braam_add_program stamps. A source's path, relative to here, names its object.

set(VCC_HOST ${CMAKE_BINARY_DIR}/host-cc/stage CACHE PATH "the host compiler's installation")
if(NOT EXISTS ${VCC_HOST}/bin/vcc)
    message(FATAL_ERROR "no host compiler at ${VCC_HOST}: use `make`, which builds it")
endif()

set(VCC ${VCC_HOST}/bin/vcc)
set(VCC_PASSES ${VCC} ${VCC_HOST}/bin/vcpp ${VCC_HOST}/bin/vparse ${VCC_HOST}/bin/vlower
    ${VCC_HOST}/bin/vgenwasm)
file(GLOB_RECURSE VCC_HEADERS CONFIGURE_DEPENDS ${VCC_HOST}/share/vcc/wasm32-braam/include/*.h)

# The assembler, as the driver runs it.
separate_arguments(VCC_FEATURES UNIX_COMMAND "${BRAAM_WASM_FEATURES}")
set(VCC_AS ${BRAAM_CC} --target=wasm32 --no-default-config ${VCC_FEATURES} -c)

# The linker, and its flags: the driver's for wasm32-braam (cc/cc.c). The top
# Makefile names devel/wasm's ld, run on the host, so that the programs are
# what cc links on Braam, where ld stamps them; wasm-ld would add a start
# function zeroing .bss, which Braam's fresh memory makes needless.
set(VCC_LD "" CACHE FILEPATH "devel/wasm's ld on the host; empty for wasm-ld and stamp.py")
if(VCC_LD)
    set(VCC_LD_DEPENDS ${VCC_LD}.wasm)
elseif(BRAAM_LD)
    set(VCC_LD ${BRAAM_LD})
    set(VCC_LD_DEPENDS ${BRAAM_LD}.wasm)
else()
    set(VCC_LD ${BRAAM_WASM_LD})
    set(VCC_LD_DEPENDS "")
endif()
set(VCC_LD_FLAGS --no-entry --import-memory --stack-first --gc-sections)

set(VCC_LIB ${CMAKE_CURRENT_BINARY_DIR}/lib)
file(MAKE_DIRECTORY ${VCC_LIB})

# Every compile runs in this directory and names its source relative to it, so
# what __FILE__ puts in an object does not depend on where the tree is.
set(VCC_TREE ${CMAKE_CURRENT_SOURCE_DIR})

# The same build as commands for Braam's shell, selfhost.sh, which
# test/selfhost.mjs runs there: in this directory, with the outputs under $O
# where this build has them under its binary directory, and the SDK's math
# sources at $T/math; $T is this directory.
set_property(GLOBAL PROPERTY VCC_SELFHOST "")
function(vcc_selfhost line)
    string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}" "$O" line "${line}")
    set_property(GLOBAL APPEND_STRING PROPERTY VCC_SELFHOST "${line}\n")
endfunction()

# <src> to <out>/<name>.o through the driver, with the driver's <flags>, and
# the object onto the list named by <list>.
function(vcc_compile list out src name flags)
    get_filename_component(src ${src} ABSOLUTE)
    file(RELATIVE_PATH rel ${VCC_TREE} ${src})
    if(rel MATCHES "^\\.\\.")
        set(rel ${src}) # generated in the binary directory
    endif()
    add_custom_command(OUTPUT ${out}/${name}.o
        COMMAND ${VCC} ${flags} -S -o ${out}/${name}.s ${rel}
        COMMAND ${VCC_AS} -o ${out}/${name}.o ${out}/${name}.s
        DEPENDS ${src} ${VCC_PASSES} ${VCC_HEADERS}
        WORKING_DIRECTORY ${VCC_TREE}
        COMMENT "Compiling ${name}.c (wasm32-braam)"
        VERBATIM)
    string(JOIN " " f ${flags})
    vcc_selfhost("cc ${f} -S -o ${out}/${name}.s ${rel} && as -o ${out}/${name}.o ${out}/${name}.s")
    set(${list} ${${list}} ${out}/${name}.o PARENT_SCOPE)
endfunction()

# The runtime, as upstream's libc/wasm32/CMakeLists.txt has it: crt0.o is
# Braam's exports; libc.a is wasm32's common and assembly sources, compiled with
# their own headers, and braam/'s, compiled with Braam's. Upstream compiles the
# common ones for plain wasm32; the driver's wasm32-braam gives the same bytes,
# as none of them has a main or asks for __braam__ but doprnt.c, which wants it.
# The float conversions and <math.h> are musl's, the SDK's share/braam/src/math,
# compiled here in place of upstream's ILP32 and assembly libm.
set(src ${CMAKE_CURRENT_SOURCE_DIR}/libc)
set(out ${CMAKE_CURRENT_BINARY_DIR}/libc)
file(MAKE_DIRECTORY ${out})
vcc_selfhost("mkdir -p ${VCC_LIB} ${out}")
set(VCC_WASM32_INCLUDE -Ilibc/wasm32/include -Ilibc/ilp32/include -Ilibc/common/include)

add_custom_command(OUTPUT ${VCC_LIB}/crt0.o
    COMMAND ${VCC_AS} -o ${VCC_LIB}/crt0.o libc/wasm32/braam/exports.s
    DEPENDS ${src}/wasm32/braam/exports.s
    WORKING_DIRECTORY ${VCC_TREE}
    VERBATIM)
vcc_selfhost("as -o ${VCC_LIB}/crt0.o libc/wasm32/braam/exports.s")

set(VCC_LIBC_OBJS "")
foreach(name memory)
    add_custom_command(OUTPUT ${out}/${name}.o
        COMMAND ${VCC_AS} -o ${out}/${name}.o libc/wasm32/${name}.s
        DEPENDS ${src}/wasm32/${name}.s
        WORKING_DIRECTORY ${VCC_TREE}
        VERBATIM)
    vcc_selfhost("as -o ${out}/${name}.o libc/wasm32/${name}.s")
    list(APPEND VCC_LIBC_OBJS ${out}/${name}.o)
endforeach()


# printf, putchar and puts are stdio.c's, coroutines on Braam.
foreach(name ${VCC_LIBC_COMMON})
    vcc_compile(VCC_LIBC_OBJS ${out} ${src}/common/${name}.c ${name}
                "-nostdinc;${VCC_WASM32_INCLUDE}")
endforeach()
foreach(name ${VCC_LIBC_BRAAM})
    vcc_compile(VCC_LIBC_OBJS ${out} ${src}/wasm32/braam/${name}.c ${name} "")
endforeach()
# The printf engine sees __braam__, which hands its floats to musl's fmt_fp.
vcc_compile(VCC_LIBC_OBJS ${out} ${src}/common/doprnt.c doprnt "")

# The math library: the one-instruction functions in assembly, and musl's C from
# the SDK, compiled in its directory as math/<dir>/<name>.c, which is where a
# Braam build has it too. libc/wasm32/braam/musl/math.h is what the SDK's
# musl_prologue.h is to clang; sincos.c and jn.c each need a word more (MUSL_*).
add_custom_command(OUTPUT ${out}/math.o
    COMMAND ${VCC_AS} -o ${out}/math.o libc/wasm32/math.s
    DEPENDS ${src}/wasm32/math.s
    WORKING_DIRECTORY ${VCC_TREE}
    VERBATIM)
vcc_selfhost("as -o ${out}/math.o libc/wasm32/math.s")
set(VCC_MATH_OBJS ${out}/math.o)

get_filename_component(VCC_MATH_ROOT ${BRAAM_EXAMPLES_DIR}/../src ABSOLUTE)
file(GLOB VCC_MATH_SOURCES RELATIVE ${VCC_MATH_ROOT}
     ${VCC_MATH_ROOT}/math/musl/*.c ${VCC_MATH_ROOT}/math/cvt/*.c)
file(GLOB VCC_MATH_HEADERS ${VCC_MATH_ROOT}/math/musl/*.h ${VCC_MATH_ROOT}/math/cvt/*.h)
set(VCC_MUSL_INCLUDE ${src}/wasm32/braam/musl)
file(MAKE_DIRECTORY ${out}/math)
vcc_selfhost("mkdir -p ${out}/math")
foreach(s ${VCC_MATH_SOURCES})
    get_filename_component(name ${s} NAME_WE)
    set(flags -Dhidden=)
    if(name MATCHES "^sincosf?$")
        list(APPEND flags -DMUSL_SINCOS)
    elseif(name MATCHES "^jnf?$")
        list(APPEND flags -DMUSL_JN)
    endif()
    set(o ${out}/math/${name})
    add_custom_command(OUTPUT ${o}.o
        COMMAND ${VCC} -I${VCC_MUSL_INCLUDE} ${flags} -S -o ${o}.s ${s}
        COMMAND ${VCC_AS} -o ${o}.o ${o}.s
        DEPENDS ${VCC_MATH_ROOT}/${s} ${VCC_MATH_HEADERS} ${VCC_MUSL_INCLUDE}/math.h
                ${VCC_PASSES} ${VCC_HEADERS}
        WORKING_DIRECTORY ${VCC_MATH_ROOT}
        COMMENT "Compiling ${s} (wasm32-braam)"
        VERBATIM)
    string(JOIN " " f ${flags})
    vcc_selfhost("cc -Ilibc/wasm32/braam/musl ${f} -S -o ${o}.s ${s} && as -o ${o}.o ${o}.s")
    list(APPEND VCC_MATH_OBJS ${o}.o)
endforeach()

add_custom_command(OUTPUT ${VCC_LIB}/libc.a
    COMMAND ${CMAKE_COMMAND} -E rm -f ${VCC_LIB}/libc.a
    COMMAND ${CMAKE_AR} qc ${VCC_LIB}/libc.a ${VCC_LIBC_OBJS}
    COMMAND ${CMAKE_AR} qcs ${VCC_LIB}/libc.a ${VCC_MATH_OBJS}
    DEPENDS ${VCC_LIBC_OBJS} ${VCC_MATH_OBJS}
    VERBATIM)
string(JOIN " " objs ${VCC_LIBC_OBJS})
string(JOIN " " mobjs ${VCC_MATH_OBJS})
vcc_selfhost("ar qc ${VCC_LIB}/libc.a ${objs}")
vcc_selfhost("ar qcs ${VCC_LIB}/libc.a ${mobjs}")

# braam.h's and errno.h's numbers against the SDK's kernel.
get_filename_component(VCC_KERNEL_INCLUDE ${BRAAM_SYSABI} DIRECTORY)
set(VCC_BRAAM_INCLUDE ${src}/wasm32/braam/include)
add_custom_command(OUTPUT ${out}/abi.ok
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_braam_abi.py
            ${VCC_BRAAM_INCLUDE}/braam.h ${BRAAM_SYSABI} ${VCC_KERNEL_INCLUDE}/result.h
            ${VCC_BRAAM_INCLUDE}/errno.h
    COMMAND ${CMAKE_COMMAND} -E touch ${out}/abi.ok
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_braam_abi.py ${VCC_BRAAM_INCLUDE}/braam.h
            ${VCC_BRAAM_INCLUDE}/errno.h ${BRAAM_SYSABI} ${VCC_KERNEL_INCLUDE}/result.h
    VERBATIM)

add_custom_target(vcc_libc ALL DEPENDS ${VCC_LIB}/crt0.o ${VCC_LIB}/libc.a ${out}/abi.ok)

function(vcc_add_program)
    cmake_parse_arguments(P "" "NAME;STACK;PAGES;TASK_BYTES" "SOURCES;FLAGS" ${ARGN})
    if("${P_NAME}" STREQUAL "" OR "${P_SOURCES}" STREQUAL "")
        message(FATAL_ERROR "vcc_add_program: NAME and SOURCES are required")
    endif()
    if(NOT P_STACK)
        set(P_STACK 131072) # the driver's
    endif()
    if(NOT P_PAGES)
        set(P_PAGES ${BRAAM_BIN_INITIAL_PAGES})
    endif()
    math(EXPR bytes "${P_PAGES} * 65536")
    set(out ${CMAKE_CURRENT_BINARY_DIR}/${P_NAME}.dir)
    file(MAKE_DIRECTORY ${out})
    vcc_selfhost("mkdir -p ${out}")
    set(objs "")
    foreach(s ${P_SOURCES})
        string(REGEX REPLACE "\\.c$" "" name ${s})
        string(REPLACE "/" "_" name ${name})
        vcc_compile(objs ${out} ${s} ${name} "${P_FLAGS}")
    endforeach()
    if(P_TASK_BYTES)
        file(WRITE ${out}/taskbytes.c "const unsigned __braam_task_bytes = ${P_TASK_BYTES};\n")
        vcc_compile(objs ${out} ${out}/taskbytes.c taskbytes "")
    endif()
    set(wasm ${CMAKE_CURRENT_BINARY_DIR}/${P_NAME}.wasm)
    # devel/wasm's ld stamps the section itself; after wasm-ld, stamp.py does.
    if(VCC_LD_DEPENDS)
        set(stamp --braam-pages=${P_PAGES},${BRAAM_BIN_MAX_PAGES})
        set(restamp "")
    else()
        set(stamp "")
        set(restamp COMMAND ${Python3_EXECUTABLE} ${BRAAM_STAMP} ${wasm}
                    --sysabi ${BRAAM_SYSABI} --initial-pages ${P_PAGES}
                    --max-pages ${BRAAM_BIN_MAX_PAGES})
    endif()
    add_custom_command(OUTPUT ${wasm}
        COMMAND ${VCC_LD} ${VCC_LD_FLAGS} -z stack-size=${P_STACK} --initial-memory=${bytes}
                ${stamp} -o ${wasm}
                ${VCC_LIB}/crt0.o ${objs} ${VCC_LIB}/libc.a
        ${restamp}
        DEPENDS ${objs} ${VCC_LIB}/crt0.o ${VCC_LIB}/libc.a ${VCC_LD_DEPENDS}
        COMMENT "Linking ${P_NAME}.wasm"
        VERBATIM)
    string(JOIN " " o ${objs})
    string(JOIN " " f ${VCC_LD_FLAGS})
    vcc_selfhost("ld ${f} -z stack-size=${P_STACK} --initial-memory=${bytes} \
--braam-pages=${P_PAGES},${BRAAM_BIN_MAX_PAGES} -o ${wasm} ${VCC_LIB}/crt0.o ${o} ${VCC_LIB}/libc.a")
    add_custom_target(bin_${P_NAME} ALL DEPENDS ${wasm})
    add_dependencies(bin_${P_NAME} vcc_libc)
endfunction()

# The compiler's passes, in C with coroutines (host.h). Their own names include
# defer and yield, which <coro.h> would otherwise take.
include(${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake)
set(VCC_PASS_FLAGS -D_CORO_NO_DEFER_YIELD)
foreach(dir ${VCC_INCLUDE_DIRS})
    list(APPEND VCC_PASS_FLAGS -I${dir})
endforeach()
vcc_add_program(NAME parse SOURCES ${VCC_PARSE_SOURCES} FLAGS ${VCC_PASS_FLAGS} STACK 1048576
                PAGES 32)
vcc_add_program(NAME lower SOURCES ${VCC_LOWER_SOURCES} FLAGS ${VCC_PASS_FLAGS} STACK 1048576
                PAGES 32)
vcc_add_program(NAME genwasm SOURCES ${VCC_GENWASM_SOURCES} FLAGS ${VCC_PASS_FLAGS}
                STACK 1048576 PAGES 32)
vcc_add_program(NAME cpp SOURCES ${VCC_CPP_SOURCES} FLAGS ${VCC_PASS_FLAGS} STACK 1048576
                PAGES 48)
vcc_add_program(NAME vcc SOURCES ${VCC_CC_SOURCES} FLAGS ${VCC_PASS_FLAGS})

get_property(VCC_SELFHOST GLOBAL PROPERTY VCC_SELFHOST)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/selfhost.sh
     CONTENT "# The build of the compiler and its runtime, for Braam (braam.cmake).\n${VCC_SELFHOST}")

# The package: the driver as cc, its passes in libexec/, the runtime where the
# driver looks for it, ../share/vcc/wasm32-braam/, and in share/ the examples
# and the documents.
set(VCC_PKG_FILES ${CMAKE_CURRENT_BINARY_DIR}/vcc.wasm=bin/cc
                  ${CMAKE_CURRENT_BINARY_DIR}/cpp.wasm=libexec/vcpp
                  ${CMAKE_CURRENT_BINARY_DIR}/parse.wasm=libexec/vparse
                  ${CMAKE_CURRENT_BINARY_DIR}/lower.wasm=libexec/vlower
                  ${CMAKE_CURRENT_BINARY_DIR}/genwasm.wasm=libexec/vgenwasm
                  ${VCC_LIB}/crt0.o=share/vcc/wasm32-braam/lib/crt0.o
                  ${VCC_LIB}/libc.a=share/vcc/wasm32-braam/lib/libc.a)
foreach(h ${VCC_HEADERS})
    file(RELATIVE_PATH rel ${VCC_HOST}/share ${h})
    list(APPEND VCC_PKG_FILES ${h}=share/${rel})
endforeach()
foreach(f docs/examples/hello.c docs/examples/notes.c Manual.md README.md
          docs/Coroutines_in_C.md)
    get_filename_component(n ${f} NAME)
    list(APPEND VCC_PKG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${f}=share/${n})
endforeach()
braam_add_package(NAME cc VERSION 0.1-r2
                  FIELD "T=vcc, a C11 compiler with coroutines, for Braam"
                  FIELD D=wasm
                  FILES ${VCC_PKG_FILES})
if(TARGET packages)
    add_dependencies(packages pkg_cc)
endif()
add_dependencies(pkg_cc vcc_libc bin_vcc bin_cpp bin_parse bin_lower bin_genwasm)

vcc_add_program(NAME hello SOURCES test/hello.c)
vcc_add_program(NAME libctest SOURCES test/libc.c)
vcc_add_program(NAME systest SOURCES test/sys.c)
