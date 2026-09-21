cmake_minimum_required(VERSION 3.24)

project(c4 LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# PORT is malloc, memset, memcmp and the rest of Group A. NOFLOAT: C4 formats
# no float, and printf itself is a buffer the driver drains rather than the
# kit's b_printf. The two assignment-as-condition sites are upstream's.
braam_add_program(NAME c4 SOURCES c4.cpp braam.cpp PORT NOFLOAT)
target_compile_options(bin_c4 PRIVATE -Wno-parentheses)

# The original sources, which c4 compiles: a bare name is looked for here
# through /pkg/bin, so `c4 hello.c` and `c4 c4.c hello.c` work after install.
set(C4_SHARE "")
file(GLOB C4_SHARE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/share/*)
foreach(f ${C4_SHARE_FILES})
    get_filename_component(n ${f} NAME)
    list(APPEND C4_SHARE ${f}=share/${n})
endforeach()

braam_add_package(NAME c4 VERSION 1.0-r0
                  FIELD "T=C4, C in four functions"
                  FILES $<TARGET_FILE:bin_c4>=bin/c4
                        ${C4_SHARE})

if(TARGET packages)
    add_dependencies(packages pkg_c4)
endif()
