# BESM-6 simulator, on its way to Braam.
#
# This is the *host* build, which exists so that every step of the port is
# proved under tests/unix.exp before the wasm target arrives.  The Braam build
# replaces this file; the top-level braam-apps CMakeLists.txt does not add this
# directory yet.

cmake_minimum_required(VERSION 3.20)

project(simbesm LANGUAGES CXX)

option(SIMBESM_WERROR "Treat warnings as errors" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- sources ----------------------------------------------------------------

# The SIMH framework, cut to what BESM-6 reaches.  It is dissolved in stages;
# what survives ends up in machine.cpp.
set(SIM_SOURCES
    simh/scp.cpp
    simh/sim_console.cpp
    simh/sim_timer.cpp)

set(BESM6_SOURCES
    besm6_arith.cpp
    debug.cpp
    besm6_cpu.cpp
    besm6_disk.cpp
    besm6_drum.cpp
    besm6_gost.cpp
    besm6_main.cpp
    besm6_mmu.cpp
    besm6_sys.cpp
    besm6_trace.cpp
    besm6_tty.cpp)

add_executable(besm6 ${SIM_SOURCES} ${BESM6_SOURCES})

target_include_directories(besm6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/simh
                                         ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(besm6 PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)

# --- host features ----------------------------------------------------------

target_compile_options(besm6 PRIVATE -Wall)

if(SIMBESM_WERROR)
    target_compile_options(besm6 PRIVATE -Werror)
endif()

# --- libraries --------------------------------------------------------------

find_library(M_LIB m)
if(M_LIB)
    target_link_libraries(besm6 PRIVATE ${M_LIB})
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(besm6 PRIVATE rt)
endif()

# --- tests ------------------------------------------------------------------

# The simulator drives the console, so the test needs a pty: expect spawns it,
# types at the Unix shell and checks what comes back.
enable_testing()
find_program(EXPECT_PROG expect)
if(EXPECT_PROG)
    add_test(NAME unix
             COMMAND ${EXPECT_PROG} -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/unix.exp)
    set_tests_properties(unix PROPERTIES
        FAIL_REGULAR_EXPRESSION "FAIL"
        TIMEOUT 300)
else()
    message(STATUS "expect not found -- the unix boot test will not be run")
endif()
