cmake_minimum_required(VERSION 3.24)

project(bzip2 LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# PORT links bzip2 through braam::compat; stream I/O uses bz_stream, not BZFILE.
braam_add_program(NAME bzip2 SOURCES braam.cpp bzip2.cpp PORT)

braam_add_package(NAME bzip2 VERSION 1.0.8-r0
                  FIELD "T=bzip2 1.0.8"
                  FILES $<TARGET_FILE:bin_bzip2>=bin/bzip2
                        $<TARGET_FILE:bin_bzip2>=bin/bunzip2
                        $<TARGET_FILE:bin_bzip2>=bin/bzcat)
add_dependencies(packages pkg_bzip2)
