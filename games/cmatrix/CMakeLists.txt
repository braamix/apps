cmake_minimum_required(VERSION 3.24)

project(cmatrix LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# No PORT and no LIBS: the rain is integers and the screen is a ProcScreen.
braam_add_program(NAME cmatrix SOURCES cmatrix.cpp)

braam_add_package(NAME cmatrix VERSION 1.2a-r0
                  FIELD "T=ASCII Matrix screensaver"
                  FILES $<TARGET_FILE:bin_cmatrix>=bin/cmatrix)

if(TARGET packages)
    add_dependencies(packages pkg_cmatrix)
endif()
