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
add_subdirectory(size)
add_subdirectory(nm)
add_subdirectory(disasm)
add_subdirectory(as)
add_subdirectory(examples)

# The examples, as share/: programs in WebAssembly text and their build.sh.
file(GLOB WASM_EXAMPLES_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.s
                                              ${CMAKE_CURRENT_SOURCE_DIR}/examples/build.sh)
set(WASM_EXAMPLES "")
foreach(f ${WASM_EXAMPLES_SRC})
    get_filename_component(n ${f} NAME)
    list(APPEND WASM_EXAMPLES ${f}=share/${n})
endforeach()

braam_add_package(NAME wasm VERSION 0.9-r0
                  FIELD "T=WebAssembly tools: ld, strip, ar, size, nm, disasm and as"
                  FILES ${WASM_EXAMPLES}
                        ${CMAKE_CURRENT_BINARY_DIR}/examples/crt.o=lib/crt.o
                        ${CMAKE_CURRENT_BINARY_DIR}/examples/libw.a=lib/libw.a
                        $<TARGET_FILE:bin_ld>=bin/ld
                        $<TARGET_FILE:bin_strip>=bin/strip
                        $<TARGET_FILE:bin_ar>=bin/ar
                        $<TARGET_FILE:bin_size>=bin/size
                        $<TARGET_FILE:bin_nm>=bin/nm
                        $<TARGET_FILE:bin_disasm>=bin/disasm
                        $<TARGET_FILE:bin_as>=bin/as)

if(TARGET packages)
    add_dependencies(packages pkg_wasm)
endif()
add_dependencies(pkg_wasm wasm_lib)
