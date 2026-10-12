cmake_minimum_required(VERSION 3.24)

include(${CMAKE_CURRENT_LIST_DIR}/sources.cmake)

# Under the tree's own CMakeLists this is the wasm32 half: the runtime and the
# programs, built by the host compiler below, configured apart (VCC_HOST).
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    include(braam.cmake)
    return()
endif()

# The compiler on the host, built by the host's C compiler: the cross compiler
# that builds this package (Plan.md, decision 1). Configure it without the
# SDK's toolchain file, in a build directory of its own.
project(cc LANGUAGES C)

# Apple's linker warns at a static library named twice.
cmake_policy(SET CMP0156 NEW)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow")

add_library(libutil STATIC ${VCC_LIBUTIL})
target_include_directories(libutil PUBLIC libutil)
target_link_libraries(libutil m)

add_library(ast STATIC ${VCC_AST})
target_include_directories(ast PUBLIC ast)
target_link_libraries(ast libutil)

add_library(scanner STATIC ${VCC_SCANNER})
target_include_directories(scanner PUBLIC scanner)
target_link_libraries(scanner PUBLIC libutil)

add_library(parser STATIC ${VCC_PARSER})
target_include_directories(parser PUBLIC parser)
target_link_libraries(parser scanner ast libutil)

add_library(tac STATIC ${VCC_TAC})
target_include_directories(tac PUBLIC tac)
target_link_libraries(tac libutil)

add_library(semantic STATIC ${VCC_SEMANTIC})
target_include_directories(semantic PUBLIC semantic)
target_link_libraries(semantic tac ast libutil)

add_library(optimize STATIC ${VCC_OPTIMIZE})
target_include_directories(optimize PUBLIC optimize)
target_link_libraries(optimize tac libutil semantic)

add_library(translator STATIC ${VCC_TRANSLATOR})
target_include_directories(translator PUBLIC translator)
target_link_libraries(translator optimize semantic tac ast libutil)

add_library(backend_common STATIC ${VCC_BACKEND_COMMON})
target_include_directories(backend_common PUBLIC backend/common)
target_link_libraries(backend_common tac libutil)

add_library(wasm STATIC ${VCC_WASM})
target_include_directories(wasm PUBLIC backend/wasm)
target_link_libraries(wasm backend_common tac libutil)

add_executable(cpp ${VCC_CPP})
target_include_directories(cpp PRIVATE cpp)
target_compile_options(cpp PRIVATE -fno-common)
target_link_libraries(cpp libutil)
target_compile_definitions(cpp PRIVATE VCC_SHARE_DIR="${CMAKE_BINARY_DIR}/share/vcc")

add_executable(parse parser/main.c)
target_link_libraries(parse parser scanner)

add_executable(lower translator/main.c)
target_link_libraries(lower optimize translator)

add_executable(genwasm backend/wasm/main.c)
target_link_libraries(genwasm wasm backend_common)

add_executable(cc cc/cc.c)

# test/libc.c on the host's C library, which Braam's is held to (test/libc.mjs).
add_executable(libctest test/libc.c)

# An installation, as upstream lays it out: the driver finds its passes beside
# it and the headers in ../share/vcc/wasm32-braam/include. Braam's headers
# first, then wasm32's, ILP32's and the common ones, the first of a name winning.
install(PROGRAMS $<TARGET_FILE:cc> DESTINATION bin RENAME vcc)
install(PROGRAMS $<TARGET_FILE:cpp> DESTINATION bin RENAME vcpp)
install(PROGRAMS $<TARGET_FILE:parse> DESTINATION bin RENAME vparse)
install(PROGRAMS $<TARGET_FILE:lower> DESTINATION bin RENAME vlower)
install(PROGRAMS $<TARGET_FILE:genwasm> DESTINATION bin RENAME vgenwasm)

set(VCC_HEADERS "")
set(VCC_HEADER_NAMES "")
foreach(dir libc/wasm32/braam/include libc/wasm32/include libc/ilp32/include
            libc/common/include)
    file(GLOB hs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.h)
    foreach(h ${hs})
        get_filename_component(name ${h} NAME)
        if(NOT name IN_LIST VCC_HEADER_NAMES)
            list(APPEND VCC_HEADER_NAMES ${name})
            list(APPEND VCC_HEADERS ${h})
        endif()
    endforeach()
endforeach()
file(GLOB VCC_SYS_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/libc/wasm32/braam/include/sys/*.h)
install(FILES ${VCC_HEADERS} DESTINATION share/vcc/wasm32-braam/include)
install(FILES ${VCC_SYS_HEADERS} DESTINATION share/vcc/wasm32-braam/include/sys)
