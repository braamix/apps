cmake_minimum_required(VERSION 3.24)

project(wlink LANGUAGES C CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

braam_add_program(NAME wlink SOURCES wasm.cpp reader.cpp dump.cpp braam.cpp)

# The fixtures the tests link, and wasm-ld's links of them to compare against.
add_subdirectory(test)
