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

braam_add_program(NAME disasm
                  SOURCES opcodes.cpp code.cpp disasm.cpp driver.cpp braam.cpp
                  LIBS wasmobj)
