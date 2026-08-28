cmake_minimum_required(VERSION 3.24)

project(le LANGUAGES C CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

if(NOT Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

# Four tables upstream generated with perl. The scripts are Python here and
# emit the same bytes; the tree already needs Python 3 and does not need perl.
set(GEN ${CMAKE_CURRENT_BINARY_DIR})
foreach(g action-enum action-name-func keymap mainmenu)
    if(g STREQUAL "action-enum" OR g STREQUAL "action-name-func")
        set(src ${CMAKE_CURRENT_SOURCE_DIR}/action-name-func)
    elseif(g STREQUAL "keymap")
        set(src ${CMAKE_CURRENT_SOURCE_DIR}/share/keymap)
    else()
        set(src ${CMAKE_CURRENT_SOURCE_DIR}/share/mainmenu)
    endif()
    if(g STREQUAL "keymap" OR g STREQUAL "mainmenu")
        set(out ${GEN}/${g}-default.h)
    else()
        set(out ${GEN}/${g}.h)
    endif()
    add_custom_command(OUTPUT ${out}
                       COMMAND ${Python3_EXECUTABLE}
                               ${CMAKE_CURRENT_SOURCE_DIR}/make-${g}.py ${src} ${out}
                       DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/make-${g}.py ${src}
                       COMMENT "Generating ${g}"
                       VERBATIM)
    list(APPEND LE_GENERATED ${out})
endforeach()

# braam.cpp supplies the C library. Each name needs -fno-builtin, or clang
# folds a call into inline code, or turns the definition into a call to itself.
set(LE_NOBUILTIN
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset -fno-builtin-memcmp
    -fno-builtin-memchr
    -fno-builtin-strlen -fno-builtin-strcpy -fno-builtin-strncpy -fno-builtin-strcat
    -fno-builtin-strncat -fno-builtin-strcmp -fno-builtin-strncmp -fno-builtin-strchr
    -fno-builtin-strrchr -fno-builtin-strstr -fno-builtin-strdup -fno-builtin-strspn
    -fno-builtin-strcspn -fno-builtin-strpbrk -fno-builtin-strtok
    -fno-builtin-strcasecmp -fno-builtin-strncasecmp
    -fno-builtin-toupper -fno-builtin-tolower -fno-builtin-isspace -fno-builtin-isblank
    -fno-builtin-isdigit -fno-builtin-isxdigit -fno-builtin-isalpha -fno-builtin-isalnum
    -fno-builtin-islower -fno-builtin-isupper -fno-builtin-isprint -fno-builtin-isgraph
    -fno-builtin-ispunct -fno-builtin-iscntrl
    -fno-builtin-getenv -fno-builtin-atoi -fno-builtin-abs -fno-builtin-labs
    -fno-builtin-strtol -fno-builtin-strtoul -fno-builtin-strtoll -fno-builtin-strtoull
    -fno-builtin-qsort -fno-builtin-fnmatch
    -fno-builtin-snprintf -fno-builtin-sprintf -fno-builtin-vsnprintf)

# Ported code keeps upstream's identifiers, its casts and its unused arguments.
# An INTERFACE target rather than target_compile_options, because braam::flags
# carries -Wall -Wextra -Wshadow and a library's options land after the
# target's own -- so nothing else would come last.
add_library(le_quiet INTERFACE)
target_compile_options(le_quiet INTERFACE
    -Wno-shadow -Wno-sign-compare -Wno-unused-parameter
    -Wno-writable-strings -Wno-unused-but-set-variable
    -Wno-parentheses -Wno-dangling-else -Wno-char-subscripts
    -Wno-missing-field-initializers -Wno-invalid-source-encoding)

# Upstream's file split and names are kept; what went is named in README.md.
# What is new: curses.cpp is the whole of ncurses this needs, leio.cpp the
# file syscalls, lefile.cpp the stdio the config parsers make of proc/file.h,
# lewchar.cpp the UTF-8 half of wchar.h, and epath.cpp the PKGDATADIR only the
# running program can know. There is no mouse.cc, rus.cc
# or tables.cc.
braam_add_program(NAME le
                  SOURCES about.cpp block.cpp bm.cpp calc.cpp chset.cpp
                          clipbrd.cpp cmd.cpp color.cpp colormnu.cpp edit.cpp
                          editcalc.cpp ext.cpp file.cpp format.cpp frames.cpp
                          getch.cpp getstr.cpp help.cpp highli.cpp history.cpp
                          kern.cpp keymap.cpp keymapdf.cpp keymapfn.cpp
                          keynames.cpp loadsave.cpp mainmenu.cpp mb.cpp
                          menu.cpp menu1.cpp options.cpp pipe.cpp screen.cpp
                          search.cpp signals.cpp textpoin.cpp undo.cpp
                          user.cpp window.cpp
                          regex.c wcwidth.c
                          braam.cpp curses.cpp epath.cpp lefile.cpp
                          leio.cpp lewchar.cpp
                          ${LE_GENERATED}
                  LIBS braam::math)

target_include_directories(bin_le PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})
target_compile_options(bin_le PRIVATE ${LE_NOBUILTIN} -fno-threadsafe-statics)
target_link_libraries(bin_le PRIVATE le_quiet)

# regex.c and wcwidth.c are the two files that are C. cinc/ is the handful of
# libc headers they name, each forwarding to the shim; REGEX_MALLOC keeps
# regex.c off alloca.
set_source_files_properties(regex.c wcwidth.c PROPERTIES
    COMPILE_DEFINITIONS "REGEX_MALLOC=1"
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/cinc;${CMAKE_CURRENT_SOURCE_DIR};${GEN}")

# The syntax file includes these by name, so they travel with it.
file(GLOB LE_SYNTAX_D_FILES ${CMAKE_CURRENT_SOURCE_DIR}/share/syntax.d/*)
foreach(f ${LE_SYNTAX_D_FILES})
    get_filename_component(n ${f} NAME)
    list(APPEND LE_SYNTAX_D ${f}=share/le/syntax.d/${n})
endforeach()

# The data travels with the binary. Only the flat bin/ entry becomes a command;
# share/ is nested, so it is payload -- and /pkg/bin/le is the link into the
# store directory it sits in, which is how epath.cpp finds it.
braam_add_package(NAME le VERSION 1.16.8-r0
                  FIELD "T=LE 1.16.8, a block-oriented text editor"
                  FILES $<TARGET_FILE:bin_le>=bin/le
                        ${CMAKE_CURRENT_SOURCE_DIR}/share/le.hlp=share/le/le.hlp
                        ${CMAKE_CURRENT_SOURCE_DIR}/share/keymap=share/le/keymap
                        ${CMAKE_CURRENT_SOURCE_DIR}/share/mainmenu=share/le/mainmenu
                        ${CMAKE_CURRENT_SOURCE_DIR}/share/syntax=share/le/syntax
                        ${LE_SYNTAX_D})
add_dependencies(packages pkg_le)
