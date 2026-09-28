cmake_minimum_required(VERSION 3.24)

project(disasm LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()
if(NOT TARGET wasmobj)
    add_subdirectory(../lib lib)
endif()

# Symbols are read by nm's reader; -C demangles with ld's copy of LLVM's
# demangler.
set(DEMANGLE ${CMAKE_CURRENT_SOURCE_DIR}/../ld/demangle)
braam_add_program(NAME disasm
                  SOURCES opcodes.cpp code.cpp disasm.cpp driver.cpp braam.cpp
                          ../nm/symbols.cpp ${DEMANGLE}/demangle.cpp
                  LIBS wasmobj)
target_include_directories(bin_disasm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ld)
# The demangler's loops must not become calls to strlen and memcpy.
set_source_files_properties(${DEMANGLE}/demangle.cpp PROPERTIES COMPILE_OPTIONS -fno-builtin)
