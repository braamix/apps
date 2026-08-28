cmake_minimum_required(VERSION 3.24)

project(uemacs LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam.cpp supplies the C library. Each name needs -fno-builtin, or clang
# folds a call into inline code, or turns the definition into a call to itself.
set(EM_NOBUILTIN
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset -fno-builtin-memcmp
    -fno-builtin-strlen -fno-builtin-strcpy -fno-builtin-strncpy -fno-builtin-strcat
    -fno-builtin-strcmp -fno-builtin-strncmp -fno-builtin-strchr -fno-builtin-strrchr
    -fno-builtin-strstr
    -fno-builtin-toupper -fno-builtin-tolower -fno-builtin-isspace -fno-builtin-isdigit
    -fno-builtin-isalpha -fno-builtin-isalnum -fno-builtin-islower -fno-builtin-isupper
    -fno-builtin-isprint -fno-builtin-iscntrl -fno-builtin-getenv -fno-builtin-atoi
    -fno-builtin-qsort
    -fno-builtin-snprintf -fno-builtin-sprintf -fno-builtin-vsnprintf)

# Ported code keeps upstream's identifiers, its casts and its unused arguments.
# An INTERFACE target rather than target_compile_options, because braam::flags
# carries -Wall -Wextra -Wshadow and a library's options land after the
# target's own -- so nothing else would come last.
add_library(em_quiet INTERFACE)
target_compile_options(em_quiet INTERFACE
    -Wno-shadow -Wno-sign-compare -Wno-unused-parameter
    -Wno-writable-strings -Wno-unused-but-set-variable
    -Wno-parentheses -Wno-dangling-else -Wno-char-subscripts
    -Wno-missing-field-initializers)

if(NOT Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

# .emacsrc and emacs.hlp go into the binary. A package's non-bin payload lands
# under a path that carries the version, which the program would then have to
# know; fileio.cpp serves these when nothing on disk answers, so a user's own
# copy in $HOME or the cwd still wins.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rcdata.cpp
                   COMMAND ${Python3_EXECUTABLE}
                           ${CMAKE_CURRENT_SOURCE_DIR}/mkdata.py
                           ${CMAKE_CURRENT_BINARY_DIR}/rcdata.cpp
                           .emacsrc=${CMAKE_CURRENT_SOURCE_DIR}/emacs.rc
                           emacs.hlp=${CMAKE_CURRENT_SOURCE_DIR}/emacs.hlp
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mkdata.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/emacs.rc
                           ${CMAKE_CURRENT_SOURCE_DIR}/emacs.hlp
                   COMMENT "Generating rcdata.cpp"
                   VERBATIM)

# Upstream's file split and names are kept; what went is named in README.md.
# screen.cpp is tcap.c and posix.c; there is no lock.c, usage.c or wrapper.c.
braam_add_program(NAME em
                  SOURCES basic.cpp bind.cpp buffer.cpp display.cpp ebind.cpp
                          eval.cpp exec.cpp file.cpp fileio.cpp globals.cpp
                          input.cpp isearch.cpp line.cpp main.cpp names.cpp
                          random.cpp region.cpp search.cpp spawn.cpp spell.cpp
                          utf8.cpp version.cpp window.cpp word.cpp
                          screen.cpp braam.cpp
                          ${CMAKE_CURRENT_BINARY_DIR}/rcdata.cpp)

# rcdata.cpp is generated into the build directory and includes builtin.h.
target_include_directories(bin_em PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(bin_em PRIVATE ${EM_NOBUILTIN})
target_link_libraries(bin_em PRIVATE em_quiet)

braam_add_package(NAME uemacs VERSION 4.0-r0
                  FIELD "T=uEmacs/PK 4.0, the MicroEMACS screen editor"
                  FILES $<TARGET_FILE:bin_em>=bin/em)
add_dependencies(packages pkg_uemacs)
