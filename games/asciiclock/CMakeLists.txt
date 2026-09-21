cmake_minimum_required(VERSION 3.24)

project(asciiclock LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# Layer buffers are ~700 KiB of BSS; default four pages is not enough.
set(BRAAM_BIN_INITIAL_PAGES 12)
math(EXPR BRAAM_BIN_INITIAL_BYTES "${BRAAM_BIN_INITIAL_PAGES} * 65536")

braam_add_program(NAME asciiclock
                  SOURCES asciiclock.cpp screen.cpp 3d.cpp clock_digital.cpp
                          backg_snow.cpp backg_starfield.cpp backg_starfield3d.cpp
                          backg_plasma.cpp backg_matrix.cpp backg_fire.cpp
                          backg_labyrinth.cpp backg_gameoflife.cpp backg_obj3d.cpp
                          foreg_cube.cpp
                  LIBS braam::math)

target_compile_options(bin_asciiclock PRIVATE -O3)

braam_add_package(NAME asciiclock VERSION 1.0-r0
                  FIELD "T=ASCII demo clock"
                  FILES $<TARGET_FILE:bin_asciiclock>=bin/asciiclock)

if(TARGET packages)
    add_dependencies(packages pkg_asciiclock)
endif()
