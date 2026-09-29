cmake_minimum_required(VERSION 3.24)

project(as LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()
if(NOT TARGET wasmobj)
    add_subdirectory(../lib lib)
endif()

braam_add_program(NAME as SOURCES lexer.cpp number.cpp ast.cpp parser.cpp resolve.cpp valid.cpp encode.cpp as.cpp driver.cpp braam.cpp
                  LIBS wasmobj)

# test/link.mjs links WAT with C; its C half needs ld's fixture driver.
if(TARGET fx_driver)
    add_subdirectory(test/link)
endif()
