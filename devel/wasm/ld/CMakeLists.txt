cmake_minimum_required(VERSION 3.24)

project(ld LANGUAGES C CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

braam_add_program(NAME ld SOURCES wasm.cpp reader.cpp dump.cpp driver.cpp symtab.cpp gc.cpp layout.cpp writer.cpp xxh3.cpp demangle/demangle.cpp braam.cpp)
# LLVM's demangler, as upstream wrote it.
set_source_files_properties(demangle/demangle.cpp PROPERTIES COMPILE_OPTIONS
    "-fno-builtin;-Wno-shadow;-Wno-unused-parameter;-Wno-unused-variable;-Wno-sign-compare;-Wno-unused-function")

# The fixtures the tests link, and wasm-ld's links of them to compare against.
add_subdirectory(test)
