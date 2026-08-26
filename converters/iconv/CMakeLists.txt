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
    -fno-builtin-memchr -fno-builtin-bzero
    -fno-builtin-strlen -fno-builtin-strcpy -fno-builtin-strncpy
    -fno-builtin-strcmp -fno-builtin-strncmp -fno-builtin-strchr -fno-builtin-strrchr
    -fno-builtin-strstr -fno-builtin-strdup -fno-builtin-strndup
    -fno-builtin-strcasecmp -fno-builtin-strncasecmp -fno-builtin-strcasestr
    -fno-builtin-strtol -fno-builtin-strtoul -fno-builtin-atoi
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

# What citrus reads a file to do, and so becomes a coroutine: opening a
# conversion. Nothing below these converts anything.
set(ICONV_OPEN
    citrus_esdb.cpp citrus_csmapper.cpp citrus_mapper.cpp
    citrus_iconv.cpp bsd_iconv.cpp)

# The conversion modules. Every one of them is upstream's, and every one is
# ordinary synchronous code but for the mapper's init.
set(ICONV_MODULES
    modules/citrus_iconv_std.cpp modules/citrus_iconv_none.cpp
    modules/citrus_utf8.cpp modules/citrus_utf8mac.cpp modules/citrus_utf7.cpp
    modules/citrus_utf1632.cpp modules/citrus_iso2022.cpp modules/citrus_hz.cpp
    modules/citrus_viqr.cpp modules/citrus_mskanji.cpp modules/citrus_zw.cpp
    modules/citrus_big5.cpp modules/citrus_ues.cpp modules/citrus_gbk2k.cpp
    modules/citrus_dechanyu.cpp modules/citrus_euc.cpp modules/citrus_euctw.cpp
    modules/citrus_johab.cpp modules/citrus_deckanji.cpp
    modules/citrus_mapper_std.cpp modules/citrus_mapper_serial.cpp
    modules/citrus_mapper_zone.cpp modules/citrus_mapper_646.cpp
    modules/citrus_mapper_none.cpp)

# The porting layer: the C library, the two replacements for dlopen and mmap,
# and where the data lives.
set(ICONV_PORT braam.cpp citrus_module.cpp citrus_mmap.cpp citrus_paths.cpp)

# The tables. mki18n.py compiles every .src into the .mps and .esdb the
# readers expect, and copies the four index texts with their containers. The
# .src tree is the dependency: a change to one rebuilds the lot, which takes
# about four seconds.
file(GLOB_RECURSE ICONV_DATA_SRC CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/data/*.src
     ${CMAKE_CURRENT_SOURCE_DIR}/data/*.part
     ${CMAKE_CURRENT_SOURCE_DIR}/data/*.alias
     ${CMAKE_CURRENT_SOURCE_DIR}/data/*.variable
     ${CMAKE_CURRENT_SOURCE_DIR}/data/*.646)

set(ICONV_I18N ${CMAKE_CURRENT_BINARY_DIR}/i18n)

set(ICONV_MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/i18n.manifest)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/i18n.stamp
                   COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mki18n.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/data ${ICONV_I18N} ${ICONV_MANIFEST}
                   COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/i18n.stamp
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mki18n.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/mkcsmapper.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/mkesdb.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/mkdb.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/citrusdb.py
                           ${ICONV_DATA_SRC}
                   COMMENT "Compiling the i18n tables"
                   VERBATIM)
add_custom_target(iconv_i18n ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/i18n.stamp)

braam_add_program(NAME iconv
                  SOURCES iconv.cpp ${ICONV_PORT} ${ICONV_CORE} ${ICONV_OPEN}
                          ${ICONV_MODULES})
target_include_directories(bin_iconv PRIVATE ${ICONV_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR}
                                             ${CMAKE_CURRENT_SOURCE_DIR}/modules)
target_compile_options(bin_iconv PRIVATE ${ICONV_NOBUILTIN} ${ICONV_DEFS} ${ICONV_PRELUDE})
target_link_libraries(bin_iconv PRIVATE iconv_quiet)

add_dependencies(bin_iconv iconv_i18n)

# The package needs to name every payload file, and the tables do not exist
# until the build runs — so the generator is run once at configure time for its
# manifest. The glob above is CONFIGURE_DEPENDS, so adding a .src reconfigures
# and the list is made again.
if(NOT EXISTS ${ICONV_MANIFEST})
    execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mki18n.py
                            ${CMAKE_CURRENT_SOURCE_DIR}/data ${ICONV_I18N} ${ICONV_MANIFEST}
                    RESULT_VARIABLE made)
    if(NOT made EQUAL 0)
        message(FATAL_ERROR "mki18n.py could not compile the tables")
    endif()
endif()

file(STRINGS ${ICONV_MANIFEST} ICONV_I18N_FILES)
set(ICONV_PAYLOAD "")
foreach(f IN LISTS ICONV_I18N_FILES)
    list(APPEND ICONV_PAYLOAD ${ICONV_I18N}/${f}=share/i18n/${f})
endforeach()

# One package, and the data travels in it. Only the flat bin/ entry becomes a
# command; share/i18n is nested, so it is payload and nothing else. That is
# what lets the binary find its own tables: /pkg/bin/iconv is a link into the
# store directory the data sits in.
braam_add_package(NAME iconv VERSION 1.16-r1
                  FIELD "T=Citrus iconv, character set conversion"
                  FILES $<TARGET_FILE:bin_iconv>=bin/iconv ${ICONV_PAYLOAD})
add_dependencies(pkg_iconv iconv_i18n)
add_dependencies(packages pkg_iconv)
