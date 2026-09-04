cmake_minimum_required(VERSION 3.24)

project(asciiquarium LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# No PORT and no LIBS: the arithmetic is plain double and nothing wants a C
# library. The sprites are compiled in, so the package has no payload.
braam_add_program(NAME asciiquarium
                  SOURCES asciiquarium.cpp animation.cpp entity.cpp
                          environment.cpp fish.cpp special.cpp info.cpp)

braam_add_package(NAME asciiquarium VERSION 2.2.0-r1
                  FIELD "T=ASCII art aquarium animation"
                  FILES $<TARGET_FILE:bin_asciiquarium>=bin/asciiquarium)

if(TARGET packages)
    add_dependencies(packages pkg_asciiquarium)
endif()
