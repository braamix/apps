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
add_subdirectory(strip)
add_subdirectory(ar)

braam_add_package(NAME wasm VERSION 0.3-r0
                  FIELD "T=WebAssembly tools: ld, strip and ar"
                  FILES $<TARGET_FILE:bin_ld>=bin/ld
                        $<TARGET_FILE:bin_strip>=bin/strip
                        $<TARGET_FILE:bin_ar>=bin/ar)

if(TARGET packages)
    add_dependencies(packages pkg_wasm)
endif()
