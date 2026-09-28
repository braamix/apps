cmake_minimum_required(VERSION 3.24)

project(wasm LANGUAGES C CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# One line per tool in the package.
add_subdirectory(ld)
