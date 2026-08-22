cmake_minimum_required(VERSION 3.24)

project(adventure LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

if(NOT Python3_EXECUTABLE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
endif()

# glorkz goes into the binary. There is no seek syscall to index a data file
# with, and a package's non-bin payload lands under a path that carries the
# version, which the program would then have to know.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/glorkz.cpp
                   COMMAND ${Python3_EXECUTABLE}
                           ${CMAKE_CURRENT_SOURCE_DIR}/mkdata.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/glorkz
                           ${CMAKE_CURRENT_BINARY_DIR}/glorkz.cpp
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mkdata.py
                           ${CMAKE_CURRENT_SOURCE_DIR}/glorkz
                   COMMENT "Generating glorkz.cpp"
                   VERBATIM)

braam_add_program(NAME adventure
                  SOURCES adventure.cpp io.cpp braam.cpp edit.cpp
                          ${CMAKE_CURRENT_BINARY_DIR}/glorkz.cpp)

# Str's const char * constructor calls strlen, and nothing provides one.
# braam.cpp does; -fno-builtin-strlen keeps clang from turning that definition
# into a call to itself.
target_compile_options(bin_adventure PRIVATE -fno-builtin-strlen)

# The package /bin/pkg installs. bin/ is what reaches PATH, and the entry's
# leaf is the command's name.
if(COMMAND braam_add_package)
    braam_add_package(NAME adventure VERSION 1.0-r0
                      FIELD "T=Colossal Cave Adventure"
                      FILES $<TARGET_FILE:bin_adventure>=bin/adventure)
    if(TARGET packages)
        add_dependencies(packages pkg_adventure)
    endif()
endif()
