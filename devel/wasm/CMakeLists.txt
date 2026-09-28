cmake_minimum_required(VERSION 3.24)

project(wasm LANGUAGES C CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# The library first, then one line per tool in the package.
add_subdirectory(lib)
add_subdirectory(ld)

braam_add_package(NAME wasm VERSION 0.1-r0
                  FIELD "T=WebAssembly tools: ld, the linker"
                  FILES $<TARGET_FILE:bin_ld>=bin/ld)

if(TARGET packages)
    add_dependencies(packages pkg_wasm)
endif()
