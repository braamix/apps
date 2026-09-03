cmake_minimum_required(VERSION 3.24)

project(uemacs LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# What braam::portflags does not already carry. An INTERFACE target rather than
# target_compile_options, because braam::flags carries -Wall -Wextra -Wshadow
# and a library's options land after the target's own -- so nothing else would
# come last.
add_library(em_quiet INTERFACE)
target_compile_options(em_quiet INTERFACE
    -Wno-parentheses -Wno-dangling-else -Wno-char-subscripts)

# Upstream's file split and names are kept; what went is named in README.md.
# screen.cpp is tcap.c and posix.c; there is no lock.c, usage.c or wrapper.c.
# epath.cpp is new: epath.h was a table of install directories, and the one
# directory that matters now has to be found at startup.
braam_add_program(NAME em
                  SOURCES basic.cpp bind.cpp buffer.cpp display.cpp ebind.cpp
                          epath.cpp eval.cpp exec.cpp file.cpp fileio.cpp
                          globals.cpp input.cpp isearch.cpp line.cpp main.cpp
                          names.cpp random.cpp region.cpp search.cpp spawn.cpp
                          spell.cpp utf8.cpp version.cpp window.cpp word.cpp
                          screen.cpp
                  PORT NOFLOAT)

target_link_libraries(bin_em PRIVATE em_quiet)

# The data travels with the binary. Only the flat bin/ entry becomes a command;
# share/ is nested, so it is payload — and /pkg/bin/em is the link into the
# store directory it sits in, which is how epath.cpp finds it.
braam_add_package(NAME uemacs VERSION 4.0.15-r4
                  FIELD "T=uEmacs/PK 4.0.15, the MicroEMACS screen editor"
                  FILES $<TARGET_FILE:bin_em>=bin/em
                        ${CMAKE_CURRENT_SOURCE_DIR}/emacs.rc=share/emacs.rc
                        ${CMAKE_CURRENT_SOURCE_DIR}/emacs.hlp=share/emacs.hlp
                        ${CMAKE_CURRENT_SOURCE_DIR}/emacs.pdf=share/emacs.pdf
                        ${CMAKE_CURRENT_SOURCE_DIR}/UTF-8-demo.txt=share/UTF-8-demo.txt)
add_dependencies(packages pkg_uemacs)
