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

# The linker, and its flags: the driver's for wasm32-braam (cc/cc.c).
if(BRAAM_LD)
    set(VCC_LD ${BRAAM_LD})
    set(VCC_LD_DEPENDS ${BRAAM_LD}.wasm)
else()
    set(VCC_LD ${BRAAM_WASM_LD})
    set(VCC_LD_DEPENDS "")
endif()
set(VCC_LD_FLAGS --no-entry --import-memory --stack-first --gc-sections)

set(VCC_LIB ${CMAKE_CURRENT_BINARY_DIR}/lib)
file(MAKE_DIRECTORY ${VCC_LIB})

# <src> to <out>/<name>.o through the driver, with the driver's <flags>, and
# the object onto the list named by <list>.
function(vcc_compile list out src name flags)
    get_filename_component(src ${src} ABSOLUTE)
    add_custom_command(OUTPUT ${out}/${name}.o
        COMMAND ${VCC} ${flags} -S -o ${out}/${name}.s ${src}
        COMMAND ${VCC_AS} -o ${out}/${name}.o ${out}/${name}.s
        DEPENDS ${src} ${VCC_PASSES} ${VCC_HEADERS}
        COMMENT "Compiling ${name}.c (wasm32-braam)"
        VERBATIM)
    set(${list} ${${list}} ${out}/${name}.o PARENT_SCOPE)
endfunction()

# The runtime, as upstream's libc/wasm32/CMakeLists.txt builds it: crt0.o is
# Braam's exports; libc.a is wasm32's common and assembly sources, compiled for
# wasm32 with its own headers, and braam/'s, compiled by the driver with Braam's.
# The float conversions and <math.h> are musl's, the SDK's libbraam_math.a, whose
# members libc.a carries in place of upstream's ILP32 and assembly libm.
set(src ${CMAKE_CURRENT_SOURCE_DIR}/libc)
set(out ${CMAKE_CURRENT_BINARY_DIR}/libc)
file(MAKE_DIRECTORY ${out})
set(VCC_WASM32_INCLUDE -I${src}/wasm32/include -I${src}/ilp32/include -I${src}/common/include)
file(GLOB VCC_WASM32_HEADERS ${src}/wasm32/include/*.h ${src}/ilp32/include/*.h
     ${src}/common/include/*.h)

add_custom_command(OUTPUT ${VCC_LIB}/crt0.o
    COMMAND ${VCC_AS} -o ${VCC_LIB}/crt0.o ${src}/wasm32/braam/exports.s
    DEPENDS ${src}/wasm32/braam/exports.s
    VERBATIM)

set(VCC_LIBC_OBJS "")
foreach(name memory)
    add_custom_command(OUTPUT ${out}/${name}.o
        COMMAND ${VCC_AS} -o ${out}/${name}.o ${src}/wasm32/${name}.s
        DEPENDS ${src}/wasm32/${name}.s
        VERBATIM)
    list(APPEND VCC_LIBC_OBJS ${out}/${name}.o)
endforeach()

macro(vcc_compile_wasm32 dir name)
    add_custom_command(OUTPUT ${out}/${name}.o
        COMMAND ${VCC_HOST}/bin/vcpp -t wasm32 -nostdinc ${VCC_WASM32_INCLUDE}
                ${src}/${dir}/${name}.c ${out}/${name}.i
        COMMAND ${VCC_HOST}/bin/vparse ${out}/${name}.i ${out}/${name}.ast
        COMMAND ${VCC_HOST}/bin/vlower -t wasm32 ${out}/${name}.ast ${out}/${name}.tac
        COMMAND ${VCC_HOST}/bin/vgenwasm ${out}/${name}.tac ${out}/${name}.s
        COMMAND ${VCC_AS} -o ${out}/${name}.o ${out}/${name}.s
        DEPENDS ${src}/${dir}/${name}.c ${VCC_PASSES} ${VCC_WASM32_HEADERS}
        COMMENT "Compiling ${name}.c (wasm32)"
        VERBATIM)
    list(APPEND VCC_LIBC_OBJS ${out}/${name}.o)
endmacro()

foreach(name atoi bsearch co ctype float128 getopt_long gmtime memccpy memchr memcmp printf
             putchar puts qsort snprintf sprintf strcat strchr strcmp strcpy strcspn strdup
             strftime strlen strncat strncmp strncpy strndup strnlen strpbrk strrchr strspn
             strstr strtok strtok_r strtol vsnprintf vsprintf)
    vcc_compile_wasm32(common ${name})
endforeach()
foreach(name rt sys stdio malloc nan strerror strtod taskbytes)
    vcc_compile(VCC_LIBC_OBJS ${out} ${src}/wasm32/braam/${name}.c ${name} "")
endforeach()
# The printf engine sees __braam__, which hands its floats to musl's fmt_fp.
vcc_compile(VCC_LIBC_OBJS ${out} ${src}/common/doprnt.c doprnt "")

set(VCC_MATH $<TARGET_FILE:braam::math>)

add_custom_command(OUTPUT ${VCC_LIB}/libc.a
    COMMAND ${CMAKE_COMMAND} -E rm -f ${VCC_LIB}/libc.a
    COMMAND ${CMAKE_AR} qc ${VCC_LIB}/libc.a ${VCC_LIBC_OBJS}
    COMMAND ${CMAKE_AR} qcsL ${VCC_LIB}/libc.a ${VCC_MATH}
    DEPENDS ${VCC_LIBC_OBJS} ${VCC_MATH}
    VERBATIM)

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
    add_custom_command(OUTPUT ${wasm}
        COMMAND ${VCC_LD} ${VCC_LD_FLAGS} -z stack-size=${P_STACK} --initial-memory=${bytes}
                -o ${wasm}
                ${VCC_LIB}/crt0.o ${objs} ${VCC_LIB}/libc.a
        COMMAND ${Python3_EXECUTABLE} ${BRAAM_STAMP} ${wasm}
                --sysabi ${BRAAM_SYSABI}
                --initial-pages ${P_PAGES}
                --max-pages ${BRAAM_BIN_MAX_PAGES}
        DEPENDS ${objs} ${VCC_LIB}/crt0.o ${VCC_LIB}/libc.a ${VCC_LD_DEPENDS}
        COMMENT "Linking ${P_NAME}.wasm"
        VERBATIM)
    add_custom_target(bin_${P_NAME} ALL DEPENDS ${wasm})
    add_dependencies(bin_${P_NAME} vcc_libc)
endfunction()

# The compiler's passes, in C with coroutines (host.h). Their own names include
# defer and yield, which <coro.h> would otherwise take.
include(${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake)
set(VCC_PASS_FLAGS -D_CORO_NO_DEFER_YIELD)
foreach(dir ${VCC_INCLUDE_DIRS})
    list(APPEND VCC_PASS_FLAGS -I${CMAKE_CURRENT_SOURCE_DIR}/${dir})
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

vcc_add_program(NAME hello SOURCES test/hello.c)
vcc_add_program(NAME libctest SOURCES test/libc.c)
vcc_add_program(NAME systest SOURCES test/sys.c)
