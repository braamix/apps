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

# Every package, so one target builds the lot. A program attaches its own.
add_custom_target(packages)

# One line per category that holds a program.
add_subdirectory(archivers)
add_subdirectory(benchmarks)
add_subdirectory(converters)
add_subdirectory(games)
