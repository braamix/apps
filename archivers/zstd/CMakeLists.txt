cmake_minimum_required(VERSION 3.24)

project(zstd LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam::zstd is libzstd itself; PORT brings zlib and liblzma with the C library,
# which is what --format=gzip and --format=xz stand on. No liblz4, so lz4 is the
# one of upstream's four optional formats that is absent.
braam_add_program(NAME zstd
                  SOURCES braam.cpp zstdcli.cpp fileio.cpp fileio_asyncio.cpp
                          util.cpp timefn.cpp
                  LIBS braam::zstd
                  PORT)

braam_add_package(NAME zstd VERSION 1.6.0-r0
                  FIELD "T=Zstandard 1.6.0"
                  FILES $<TARGET_FILE:bin_zstd>=bin/zstd
                        $<TARGET_FILE:bin_zstd>=bin/unzstd
                        $<TARGET_FILE:bin_zstd>=bin/zstdcat
                        $<TARGET_FILE:bin_zstd>=bin/zstdmt)
add_dependencies(packages pkg_zstd)
