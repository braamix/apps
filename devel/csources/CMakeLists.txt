cmake_minimum_required(VERSION 3.24)

# vcc's own sources and a Makefile that builds them on Braam: devel/cc's
# files, at the paths they have there, with no copy kept here.
project(csources NONE)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

set(CC_TREE ${CMAKE_CURRENT_SOURCE_DIR}/../cc)
include(${CC_TREE}/sources.cmake)

# The compiler: every part's sources, each program's main, and the headers.
set(srcs ${VCC_LIBUTIL} ${VCC_AST} ${VCC_SCANNER} ${VCC_PARSER} ${VCC_TAC} ${VCC_SEMANTIC}
         ${VCC_OPTIMIZE} ${VCC_TRANSLATOR} ${VCC_BACKEND_COMMON} ${VCC_WASM} ${VCC_CPP}
         parser/main.c translator/main.c backend/wasm/main.c cc/cc.c)
foreach(dir ${VCC_INCLUDE_DIRS} cpp)
    file(GLOB hs CONFIGURE_DEPENDS RELATIVE ${CC_TREE} ${CC_TREE}/${dir}/*.h)
    list(APPEND srcs ${hs})
endforeach()

# The runtime: its C, its assembly and its headers.
foreach(name ${VCC_LIBC_COMMON} doprnt)
    list(APPEND srcs libc/common/${name}.c)
endforeach()
foreach(name ${VCC_LIBC_BRAAM})
    list(APPEND srcs libc/wasm32/braam/${name}.c)
endforeach()
list(APPEND srcs libc/wasm32/braam/exports.s libc/wasm32/memory.s libc/wasm32/math.s
                 libc/wasm32/braam/musl/math.h LICENSE)
foreach(dir libc/common/include libc/ilp32/include libc/wasm32/include
            libc/wasm32/braam/include libc/wasm32/braam/include/sys)
    file(GLOB hs CONFIGURE_DEPENDS RELATIVE ${CC_TREE} ${CC_TREE}/${dir}/*.h)
    list(APPEND srcs ${hs})
endforeach()

set(files ${CMAKE_CURRENT_SOURCE_DIR}/Makefile=Makefile
          ${CMAKE_CURRENT_SOURCE_DIR}/README.md=README.md)
foreach(f ${srcs})
    list(APPEND files ${CC_TREE}/${f}=${f})
endforeach()

# The math library's C, musl's, from the SDK: at math/, as the cc package's
# libc.a was compiled from it.
get_filename_component(math ${BRAAM_EXAMPLES_DIR}/../src/math ABSOLUTE)
file(GLOB ms CONFIGURE_DEPENDS RELATIVE ${math}
     ${math}/musl/*.c ${math}/musl/*.h ${math}/cvt/*.c ${math}/cvt/*.h)
foreach(f ${ms} musl/COPYRIGHT)
    list(APPEND files ${math}/${f}=math/${f})
endforeach()

braam_add_package(NAME csources VERSION 0.1-r3
                  FIELD "T=vcc's C sources, and a Makefile that builds the compiler on Braam"
                  FIELD "D=cc make"
                  FILES ${files})
if(TARGET packages)
    add_dependencies(packages pkg_csources)
endif()
