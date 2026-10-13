cmake_minimum_required(VERSION 3.24)

project(make LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# PORT for the C library; NOFLOAT, a time being a count of milliseconds.
braam_add_program(NAME make
                  SOURCES main.cpp doname.cpp files.cpp misc.cpp dosys.cpp ident.cpp gram.cpp
                  PORT NOFLOAT)

braam_add_package(NAME make VERSION 2.58-r2
                  FIELD "T=POSIX make, grown from V7 make"
                  FILES $<TARGET_FILE:bin_make>=bin/make)

if(TARGET packages)
    add_dependencies(packages pkg_make)
endif()
