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

# The package builder reached the SDK after 0.4.162, which is the release this
# tree pins. Until it moves, take mkpkg.py from the core tree beside this
# checkout — packaging is then the one thing here that wants a second
# repository, and only when a package is actually built.
if(NOT COMMAND braam_add_package)
    if(NOT BRAAM_MKPKG)
        find_file(BRAAM_MKPKG mkpkg.py
                  PATHS ${CMAKE_SOURCE_DIR}/../braam-core/tools
                  NO_DEFAULT_PATH
                  DOC "mkpkg.py, the package builder")
    endif()
    if(BRAAM_MKPKG)
        include(cmake/BraamPackage.cmake)
    else()
        message(STATUS
            "no mkpkg.py: this SDK ships none and ../braam-core is not there,"
            " so `make package` has nothing to build with")
    endif()
endif()

# Every package, so one target builds the lot. A program attaches its own.
if(COMMAND braam_add_package)
    add_custom_target(packages)
endif()

# One line per category that holds a program.
add_subdirectory(benchmarks)
