cmake_minimum_required(VERSION 3.24)

project(dhrystone LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam::math is for the report's %f alone: the benchmark itself is integer.
braam_add_program(NAME dhrystone
                  SOURCES dhry_1.cpp dhry_2.cpp dhry_lib.cpp
                  LIBS braam::math)

# -O3, as the original Makefile used: the toolchain defaults to MinSizeRel, and
# a benchmark built for size reports a number about size.
#
# -fno-builtin-str*: the three routines are written out in dhry_lib.cpp, and
# clang must neither fold a call to one into inline code — which is not what
# the benchmark measured — nor turn any of the three implementations into a
# call to itself.
target_compile_options(bin_dhrystone PRIVATE
    -O3
    -fno-builtin-strcpy -fno-builtin-strcmp -fno-builtin-strlen)

# The package /bin/pkg installs. bin/ is what reaches PATH, and the entry's
# leaf is the command's name — bin/dhrystone, never bin/dhrystone.wasm.
braam_add_package(NAME dhrystone VERSION 2.1-r3
                  FIELD "T=the Dhrystone 2.1 integer benchmark"
                  FILES $<TARGET_FILE:bin_dhrystone>=bin/dhrystone)
add_dependencies(packages pkg_dhrystone)
