cmake_minimum_required(VERSION 3.24)

project(gzip LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# PORT links zlib, bzip2, lzma and zstd through braam::compat; only decompress
# entry points are reached, so --gc-sections drops the zstd encoder.
braam_add_program(NAME gzip
                  SOURCES braam.cpp gzip_cli.cpp zcompress.cpp unlz_fd.cpp gzip_body.inc.cpp
                  PORT NOFLOAT)

braam_add_package(NAME gzip VERSION 20190107-r0
                  FIELD "T=FreeBSD gzip 20190107"
                  FILES $<TARGET_FILE:bin_gzip>=bin/gzip
                        $<TARGET_FILE:bin_gzip>=bin/gunzip
                        $<TARGET_FILE:bin_gzip>=bin/zcat
                        $<TARGET_FILE:bin_gzip>=bin/gzcat)
add_dependencies(packages pkg_gzip)
