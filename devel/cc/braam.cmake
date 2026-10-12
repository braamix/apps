# The wasm32 half of devel/cc, under the tree's own CMakeLists: the runtime,
# crt0.o and libc.a, and programs written in C with coroutines, all compiled
# by the host compiler in VCC_HOST and assembled and linked by the SDK's
# tools. The top Makefile builds and installs the host compiler first.
#
#   vcc_add_program(NAME hello SOURCES hello.c)
#
# defines target bin_<name> producing <name>.wasm, stamped as
# braam_add_program stamps.

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
set(VCC_LD_FLAGS --no-entry --import-memory --stack-first -z stack-size=131072 --gc-sections
    --initial-memory=${BRAAM_BIN_INITIAL_BYTES})

set(VCC_LIB ${CMAKE_CURRENT_BINARY_DIR}/lib)
file(MAKE_DIRECTORY ${VCC_LIB})

# <dir>/<name>.c to <out>/<name>.o through the driver, and the object onto
# the list named by <list>.
function(vcc_compile list out src)
    get_filename_component(name ${src} NAME_WE)
    get_filename_component(src ${src} ABSOLUTE)
    add_custom_command(OUTPUT ${out}/${name}.o
        COMMAND ${VCC} -S -o ${out}/${name}.s ${src}
        COMMAND ${VCC_AS} -o ${out}/${name}.o ${out}/${name}.s
        DEPENDS ${src} ${VCC_PASSES} ${VCC_HEADERS}
        COMMENT "Compiling ${name}.c (wasm32-braam)"
        VERBATIM)
    set(${list} ${${list}} ${out}/${name}.o PARENT_SCOPE)
endfunction()

# The runtime, as upstream's libc/wasm32/CMakeLists.txt builds it: crt0.o is
# Braam's exports; libc.a is wasm32's common, ILP32 and assembly sources,
# compiled for wasm32 with its own headers, and braam/'s, compiled by the
# driver with Braam's.
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
foreach(name memory sqrt)
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

foreach(name atoi co doprnt fabs float128 fma fmax fmin memchr memcmp printf putchar puts
             snprintf sprintf strcat strchr strcmp strcpy strlen strncat strncmp strncpy
             strrchr strstr strtok)
    vcc_compile_wasm32(common ${name})
endforeach()
foreach(name frexp ldexp modf)
    vcc_compile_wasm32(ilp32 ${name})
endforeach()
foreach(name rt sys stdio malloc strerror taskbytes)
    vcc_compile(VCC_LIBC_OBJS ${out} ${src}/wasm32/braam/${name}.c)
endforeach()

add_custom_command(OUTPUT ${VCC_LIB}/libc.a
    COMMAND ${CMAKE_COMMAND} -E rm -f ${VCC_LIB}/libc.a
    COMMAND ${CMAKE_AR} rcs ${VCC_LIB}/libc.a ${VCC_LIBC_OBJS}
    DEPENDS ${VCC_LIBC_OBJS}
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
    cmake_parse_arguments(P "" "NAME" "SOURCES" ${ARGN})
    if("${P_NAME}" STREQUAL "" OR "${P_SOURCES}" STREQUAL "")
        message(FATAL_ERROR "vcc_add_program: NAME and SOURCES are required")
    endif()
    set(out ${CMAKE_CURRENT_BINARY_DIR}/${P_NAME}.dir)
    file(MAKE_DIRECTORY ${out})
    set(objs "")
    foreach(s ${P_SOURCES})
        vcc_compile(objs ${out} ${s})
    endforeach()
    set(wasm ${CMAKE_CURRENT_BINARY_DIR}/${P_NAME}.wasm)
    add_custom_command(OUTPUT ${wasm}
        COMMAND ${VCC_LD} ${VCC_LD_FLAGS} -o ${wasm} ${VCC_LIB}/crt0.o ${objs} ${VCC_LIB}/libc.a
        COMMAND ${Python3_EXECUTABLE} ${BRAAM_STAMP} ${wasm}
                --sysabi ${BRAAM_SYSABI}
                --initial-pages ${BRAAM_BIN_INITIAL_PAGES}
                --max-pages ${BRAAM_BIN_MAX_PAGES}
        DEPENDS ${objs} ${VCC_LIB}/crt0.o ${VCC_LIB}/libc.a ${VCC_LD_DEPENDS}
        COMMENT "Linking ${P_NAME}.wasm"
        VERBATIM)
    add_custom_target(bin_${P_NAME} ALL DEPENDS ${wasm})
    add_dependencies(bin_${P_NAME} vcc_libc)
endfunction()

vcc_add_program(NAME hello SOURCES test/hello.c)
