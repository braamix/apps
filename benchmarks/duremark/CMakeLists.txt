cmake_minimum_required(VERSION 3.24)

project(duremark LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam.cpp is the porting layer, as unix.c and dos.c are upstream. braam::math
# is for its %f alone: the benchmark itself is integer.
braam_add_program(NAME duremark
                  SOURCES main.cpp list.cpp matrix.cpp state.cpp braam.cpp
                  LIBS braam::math)

# -O2, which is what upstream's own Makefile used. The toolchain defaults to
# MinSizeRel, and a benchmark built for size reports a number about size.
target_compile_options(bin_duremark PRIVATE -O2)

# The package /bin/pkg installs. bin/ is what reaches PATH, and the entry's
# leaf is the command's name.
braam_add_package(NAME duremark VERSION 1.1-r4
                  FIELD "T=a small CoreMark-inspired integer benchmark"
                  FILES $<TARGET_FILE:bin_duremark>=bin/duremark)
add_dependencies(packages pkg_duremark)
