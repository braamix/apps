cmake_minimum_required(VERSION 3.24)

project(asciifluid LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam::math is hypot and tanh; no PORT, since nothing here wants a C library.
braam_add_program(NAME asciifluid SOURCES asciifluid.cpp LIBS braam::math)

# Two O(n^2) sweeps a frame, like dhrystone.
target_compile_options(bin_asciifluid PRIVATE -O3)

# The configurations, which the program reads out of the store by name.
set(ASCIIFLUID_DATA "")
file(GLOB ASCIIFLUID_TXT ${CMAKE_CURRENT_SOURCE_DIR}/data/*.txt)
foreach(f ${ASCIIFLUID_TXT})
    get_filename_component(n ${f} NAME)
    list(APPEND ASCIIFLUID_DATA ${f}=share/${n})
endforeach()

# bin/ is what reaches PATH, and the entry's leaf is the command's name;
# share/ is nested, so it is payload the program finds through /pkg/bin.
braam_add_package(NAME asciifluid VERSION 1.0-r0
                  FIELD "T=IOCCC 2012 ASCII fluid simulator"
                  FILES $<TARGET_FILE:bin_asciifluid>=bin/asciifluid
                        ${ASCIIFLUID_DATA})

if(TARGET packages)
    add_dependencies(packages pkg_asciifluid)
endif()
