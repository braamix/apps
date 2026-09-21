cmake_minimum_required(VERSION 3.24)

project(xz LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# PORT links liblzma through braam::compat; stream I/O uses lzma_stream.
braam_add_program(NAME xz
                  SOURCES braam.cpp getopt.cpp mytime.cpp util.cpp suffix.cpp
                          options.cpp hardware.cpp message.cpp file_io.cpp
                          coder.cpp list.cpp args.cpp main.cpp
                  PORT)

braam_add_package(NAME xz VERSION 5.8.4-r2
                  FIELD "T=xz 5.8.4"
                  FILES $<TARGET_FILE:bin_xz>=bin/xz
                        $<TARGET_FILE:bin_xz>=bin/unxz
                        $<TARGET_FILE:bin_xz>=bin/xzcat
                        $<TARGET_FILE:bin_xz>=bin/lzma
                        $<TARGET_FILE:bin_xz>=bin/unlzma
                        $<TARGET_FILE:bin_xz>=bin/lzcat)
add_dependencies(packages pkg_xz)
