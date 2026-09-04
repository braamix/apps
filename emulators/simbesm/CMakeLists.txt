# The BESM-6 simulator: a Braam program, and a host build to prove it by.
#
# Under the Braam toolchain this is the program the package ships.  Configured
# without one it builds a native binary that tests/unix.exp drives, which is how
# every step of the port was verified.

cmake_minimum_required(VERSION 3.20)

project(simbesm LANGUAGES CXX)

# The package's version, and what the banner prints.  apk's grammar (1.2-r0);
# nothing checks the spelling, so read it back.
set(SIMBESM_VERSION 1.0-r3)

# The machine, which is the same in both builds.
set(BESM6_SOURCES
    besm6_arith.cpp
    besm6_cpu.cpp
    besm6_disk.cpp
    besm6_drum.cpp
    besm6_gost.cpp
    besm6_main.cpp
    besm6_mmu.cpp
    besm6_sys.cpp
    besm6_trace.cpp
    besm6_tty.cpp
    blob.cpp
    console.cpp
    machine.cpp
    sink.cpp)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    # --- the Braam program --------------------------------------------------
    if(NOT TARGET braam::proc)
        find_package(braam REQUIRED)
    endif()

    # memory[512*1024] is 4 MiB of BSS, and wasm-ld wants the initial memory to
    # cover it.  The cap is still the kernel's; this is only what the module is
    # instantiated with.
    set(BRAAM_BIN_INITIAL_PAGES 80)
    math(EXPR BRAAM_BIN_INITIAL_BYTES "${BRAAM_BIN_INITIAL_PAGES} * 65536")

    braam_add_program(NAME besm6 SOURCES ${BESM6_SOURCES} braam.cpp
                      PORT LIBS braam::math)
    target_include_directories(bin_besm6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(bin_besm6 PRIVATE -O3) # an interpreter, like dhrystone
    target_compile_definitions(bin_besm6 PRIVATE SIMBESM_VERSION="${SIMBESM_VERSION}")

    # The kernel and the two packs, which the program copies out of the store on
    # its first run: they are written in place, and the store is read-only.
    set(BESM6_DATA "")
    foreach(f unix root3072.disk usr3100.disk)
        list(APPEND BESM6_DATA "${CMAKE_CURRENT_SOURCE_DIR}/data/${f}=share/besm6/${f}")
    endforeach()

    braam_add_package(NAME simbesm VERSION ${SIMBESM_VERSION}
                      FIELD "T=BESM-6 mainframe simulator that boots Unix"
                      FILES $<TARGET_FILE:bin_besm6>=bin/besm6 ${BESM6_DATA})
    # `packages' is the top-level tree's; this directory also configures alone.
    if(TARGET packages)
        add_dependencies(packages pkg_simbesm)
    endif()
else()
    # --- the host build, which tests/unix.exp drives -------------------------
    option(SIMBESM_WERROR "Treat warnings as errors" ON)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()

    add_executable(besm6 ${BESM6_SOURCES} host.cpp)
    target_include_directories(besm6 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(besm6 PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS ON)
    target_compile_definitions(besm6 PRIVATE SIMBESM_VERSION="${SIMBESM_VERSION}")
    target_compile_options(besm6 PRIVATE -Wall)
    if(SIMBESM_WERROR)
        target_compile_options(besm6 PRIVATE -Werror)
    endif()

    find_library(M_LIB m)
    if(M_LIB)
        target_link_libraries(besm6 PRIVATE ${M_LIB})
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(besm6 PRIVATE rt)
    endif()

    # The simulator drives the console, so the test needs a pty: expect spawns
    # it, types at the Unix shell and checks what comes back.
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
endif()
