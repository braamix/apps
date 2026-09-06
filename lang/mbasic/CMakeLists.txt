cmake_minimum_required(VERSION 3.24)

project(mbasic LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# Upstream is one MACRO-10 file with 46 SUBTTL sections; the split follows
# them, and each source names the doc chapter it implements. braam::math is
# the transcendentals and ftoa's conversions -- no PORT, since this is a
# rewrite in Braam idiom and not a recompile of C.
braam_add_program(NAME mbasic
                  SOURCES braam.cpp crunch.cpp edit.cpp epath.cpp file.cpp
                          fout.cpp frmevl.cpp init.cpp input.cpp list.cpp
                          math.cpp newstt.cpp print.cpp ptrget.cpp stmt.cpp
                          string.cpp sys.cpp tables.cpp
                  LIBS braam::math)

# The examples, which LOAD finds by bare name through /pkg/bin (epath.cpp).
# share/ is nested, so it is payload rather than a command. CONFIGURE_DEPENDS
# because adding one must reconfigure.
file(GLOB MBASIC_BAS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.bas)
set(MBASIC_EXAMPLES "")
foreach(f ${MBASIC_BAS})
    get_filename_component(n ${f} NAME)
    list(APPEND MBASIC_EXAMPLES ${f}=share/${n})
endforeach()

braam_add_package(NAME mbasic VERSION 1.1-r2
                  FIELD "T=Microsoft BASIC 1.1 for the 6502"
                  FILES $<TARGET_FILE:bin_mbasic>=bin/mbasic ${MBASIC_EXAMPLES})

if(TARGET packages)
    add_dependencies(packages pkg_mbasic)
endif()
