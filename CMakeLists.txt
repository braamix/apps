cmake_minimum_required(VERSION 3.24)

project(braam_apps LANGUAGES CXX)

# The toolchain file is what makes the compiler a wasm32 one, and CMake fixes
# the compiler when a project is first configured — so a build directory
# configured without it cannot be repaired, only deleted. `make` passes it.
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    message(FATAL_ERROR
        "A Braam program builds only for freestanding wasm32, and this build "
        "tree is configured for ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}.\n"
        "Delete the build directory and use `make`, which fetches the SDK and "
        "names its toolchain file.")
endif()

find_package(braam REQUIRED)

# `make LINKER=ld`: every program linked by devel/wasm's ld, which stamps as it
# links, so stamp.py's step only checks the stamp.
set(BRAAM_LD "" CACHE FILEPATH "a linker for clang -fuse-ld=; empty for wasm-ld")
set(BRAAM_STAMP_PY ${BRAAM_STAMP})
if(BRAAM_LD)
    add_link_options(-fuse-ld=${BRAAM_LD})
    set(BRAAM_STAMP ${CMAKE_CURRENT_SOURCE_DIR}/devel/wasm/ld/stamped.py)
endif()

# Warnings, for every language and every program below.
add_compile_options(-Wall -Werror -Wshadow)

# Every package, so one target builds the lot. A program attaches its own.
add_custom_target(packages)

# One line per category that holds a program.
add_subdirectory(archivers)
add_subdirectory(benchmarks)
add_subdirectory(converters)
add_subdirectory(devel)
add_subdirectory(editors)
add_subdirectory(emulators)
add_subdirectory(games)
add_subdirectory(lang)

# Under `make LINKER=ld`, a new ld relinks every program. The wrapper at
# BRAAM_LD runs the ld.wasm beside it.
function(braam_ld_depends dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(t ${targets})
        get_target_property(type ${t} TYPE)
        if(type STREQUAL "EXECUTABLE")
            set_property(TARGET ${t} APPEND PROPERTY LINK_DEPENDS ${BRAAM_LD}.wasm)
        endif()
    endforeach()
    get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(d ${subdirs})
        braam_ld_depends(${d})
    endforeach()
endfunction()
if(BRAAM_LD)
    braam_ld_depends(${CMAKE_CURRENT_SOURCE_DIR})
endif()
