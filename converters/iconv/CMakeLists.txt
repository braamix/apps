cmake_minimum_required(VERSION 3.24)

project(iconv LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

if(NOT Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

# braam.cpp supplies the C library. Each name needs -fno-builtin, or clang
# folds a call into inline code, or turns the definition into a call to itself.
set(ICONV_NOBUILTIN
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset -fno-builtin-memcmp
    -fno-builtin-memchr
    -fno-builtin-strlen -fno-builtin-strcpy -fno-builtin-strncpy
    -fno-builtin-strcmp -fno-builtin-strncmp -fno-builtin-strchr -fno-builtin-strrchr
    -fno-builtin-strstr -fno-builtin-strdup -fno-builtin-strndup
    -fno-builtin-strcasecmp -fno-builtin-strncasecmp -fno-builtin-strcasestr
    -fno-builtin-strtol -fno-builtin-strtoul
    -fno-builtin-snprintf -fno-builtin-vsnprintf
    -fno-builtin-abort -fno-builtin-exit -fno-builtin-getenv -fno-builtin-qsort
    -fno-builtin-isspace -fno-builtin-isdigit -fno-builtin-isalpha
    -fno-builtin-isalnum -fno-builtin-toupper -fno-builtin-tolower)

# Ported code keeps upstream's identifiers, its casts and its unused arguments;
# -Wall -Wextra -Wshadow is the tree's, and these fire all over 2000s BSD C.
# An INTERFACE target rather than target_compile_options, because braam::flags
# carries the -W flags and a library's options land after the target's own.
add_library(iconv_quiet INTERFACE)
target_compile_options(iconv_quiet INTERFACE
    -Wno-shadow -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function
    -Wno-writable-strings -Wno-missing-field-initializers)

# Upstream is Apple's libiconv, and its __APPLE__ branches are the ones whose
# data format we generate — every .mps carries the transliteration table they
# read. They are selected under a macro of our own rather than by claiming to
# be a platform this is not.
set(ICONV_DEFS -D_CITRUS_APPLE)

# Upstream's headers use __BEGIN_DECLS, the BSD integer names and stdio's
# SEEK_* without including anything for them, relying on the .c file having
# done it first. clang-format sorts includes and does not know that, so the
# prelude comes in ahead of the first line rather than depending on an order.
set(ICONV_PRELUDE -include ${CMAKE_CURRENT_SOURCE_DIR}/include/prelude.h)

# include/ holds the freestanding headers citrus's #includes name: sys/queue.h
# vendored, and declarations for the C library braam.cpp supplies.
set(ICONV_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# The citrus core that needs no signature change: nothing here reaches a
# stream, because citrus reads files only while a conversion is being opened.
set(ICONV_CORE
    citrus_bcs.cpp citrus_bcs_strtol.cpp citrus_bcs_strtoul.cpp
    citrus_db.cpp citrus_db_hash.cpp citrus_hash.cpp
    citrus_memstream.cpp citrus_prop.cpp citrus_none.cpp
    citrus_stdenc.cpp citrus_lookup.cpp)

# The porting layer: the C library, the two replacements for dlopen and mmap,
# and where the data lives.
set(ICONV_PORT braam.cpp citrus_module.cpp citrus_mmap.cpp citrus_paths.cpp)

add_library(iconv_obj OBJECT ${ICONV_PORT} ${ICONV_CORE})
target_include_directories(iconv_obj PRIVATE ${ICONV_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(iconv_obj PRIVATE ${ICONV_NOBUILTIN} ${ICONV_DEFS} ${ICONV_PRELUDE})
target_link_libraries(iconv_obj PRIVATE braam::proc braam::flags iconv_quiet)
