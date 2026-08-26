cmake_minimum_required(VERSION 3.24)

project(zip LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam.cpp supplies the C library. Each name needs -fno-builtin, or clang
# folds a call into inline code, or turns the definition into a call to itself.
set(ZIP_NOBUILTIN
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset -fno-builtin-memcmp
    -fno-builtin-strlen -fno-builtin-strcpy -fno-builtin-strncpy -fno-builtin-strcat
    -fno-builtin-strcmp -fno-builtin-strncmp -fno-builtin-strchr -fno-builtin-strrchr
    -fno-builtin-strstr -fno-builtin-strncat
    # clang turns upstream's `while (wide[n]) n++` into a wcslen call, and
    # there is no wcslen to call.
    -fno-builtin-wcslen -fno-builtin-toupper -fno-builtin-tolower
    -fno-builtin-isspace -fno-builtin-isdigit -fno-builtin-isalpha -fno-builtin-isalnum -fno-builtin-isprint -fno-builtin-iscntrl -fno-builtin-getenv -fno-builtin-atoi -fno-builtin-strtol -fno-builtin-qsort)

# Ported code keeps upstream's identifiers, its casts and its unused arguments;
# -Wall -Wextra -Wshadow is the tree's, and these fire all over 1990s C that is
# doing nothing wrong. An INTERFACE target rather than target_compile_options,
# because braam::flags carries the -W flags and a library's options land after
# the target's own — so nothing else would come last.
add_library(zip_quiet INTERFACE)
target_compile_options(zip_quiet INTERFACE
    -Wno-shadow -Wno-sign-compare -Wno-unused-parameter
    -Wno-writable-strings -Wno-unused-but-set-variable)

braam_add_program(NAME zip
                  SOURCES zip.cpp zipfile.cpp zipup.cpp fileio.cpp util.cpp
                          globals.cpp crc32.cpp deflate.cpp trees.cpp
                          unix.cpp braam.cpp)

target_compile_options(bin_zip PRIVATE ${ZIP_NOBUILTIN})
target_link_libraries(bin_zip PRIVATE zip_quiet)
