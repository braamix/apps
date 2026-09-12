cmake_minimum_required(VERSION 3.24)

project(eh LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# What braam::portflags does not already carry. An INTERFACE target rather than
# target_compile_options, because braam::flags carries -Wall -Wextra -Wshadow
# and a library's options land after the target's own.
add_library(eh_quiet INTERFACE)
target_compile_options(eh_quiet INTERFACE
    -Wno-parentheses -Wno-dangling-else -Wno-char-subscripts
    -Wno-invalid-source-encoding)

# eh.cpp is upstream's single file, the IOCCC arms stripped. curses.cpp is the
# whole of curses it needs over the Grid, and getch.cpp the one place it parks.
# The POSIX ERE the C library gave it is braam::regex now, which PORT carries
# under the name <regex.h>.
braam_add_program(NAME eh
                  SOURCES eh.cpp curses.cpp getch.cpp
                  PORT NOFLOAT)

# -funsigned-char is not optional: mblength() asserts 0 <= ch < 256 and
# charwidth() compares 127 < *s && *s < 194.
#
# BUF and MODE are upstream's Makefile defaults. BUILT is its UTC build date;
# a fixed string here instead, so two builds of the same sources agree.
target_compile_options(bin_eh PRIVATE
    -funsigned-char -fno-threadsafe-statics
    -DFAST_MOVE -DNDEBUG -DBUF=131072 -DMODE=0600
    -DVERSION="v1.8.1" -DBUILT="braam")
target_link_libraries(bin_eh PRIVATE eh_quiet)

braam_add_package(NAME eh VERSION 1.8.1-r1
                  FIELD "T=EH 1.8.1, vi(1) the good parts version"
                  FILES $<TARGET_FILE:bin_eh>=bin/eh)
add_dependencies(packages pkg_eh)
