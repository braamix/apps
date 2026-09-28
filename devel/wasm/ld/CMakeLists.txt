cmake_minimum_required(VERSION 3.24)

project(ld LANGUAGES C CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()
if(NOT TARGET wasmobj)
    add_subdirectory(../lib lib)
endif()

braam_add_program(NAME ld SOURCES dump.cpp driver.cpp symtab.cpp gc.cpp layout.cpp writer.cpp xxh3.cpp demangle/demangle.cpp braam.cpp
                  LIBS wasmobj)
# The demangler's loops must not become calls to strlen and memcpy.
set_source_files_properties(demangle/demangle.cpp PROPERTIES COMPILE_OPTIONS -fno-builtin)

# The fixtures the tests link, and wasm-ld's links of them to compare against.
add_subdirectory(test)
