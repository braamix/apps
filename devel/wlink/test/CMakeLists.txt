# Fixtures: small programs, each exercising one thing a linker must get right.
# Each is compiled once to objects (and archives) that wlink's tests link, and
# linked here by wasm-ld with the usual Braam flags as the reference.
# fixtures.json names every file, so the tests need no knowledge of CMake's
# directory layout.

set(FX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# SDK archives in the order braam_add_program links them.
set(FX_LIBS braam::proc braam::ui)
set(FX_PORT_LIBS braam::compat_nofloat braam::compat_proc braam::proc braam::ui
                 braam::compat_pure braam::math braam::regex braam::zlib
                 braam::bzip2 braam::lzma braam::zstd)

# The driver: proc_main and the output buffer, shared by every fixture.
add_library(fx_driver OBJECT ${FX_SRC}/driver.cpp)
target_link_libraries(fx_driver PRIVATE braam::flags)

# An archive for the fixtures to link.
function(wlink_archive name)
    add_library(${name} STATIC ${ARGN})
    target_link_libraries(${name} PRIVATE braam::flags)
    target_include_directories(${name} PRIVATE ${FX_SRC})
endfunction()

# JSON array of quoted paths, from a genex list.
function(fx_json_list out list)
    set(${out} "[\"$<JOIN:${list},\"$<COMMA> \">\"]" PARENT_SCOPE)
endfunction()

# wlink_fixture(<name> SOURCES <src>... [ARCHIVES <target>...] [PORT])
function(wlink_fixture name)
    cmake_parse_arguments(F "PORT" "" "SOURCES;ARCHIVES" ${ARGN})

    add_library(fxo_${name} OBJECT ${F_SOURCES})
    target_link_libraries(fxo_${name} PRIVATE braam::flags)
    target_include_directories(fxo_${name} PRIVATE ${FX_SRC})
    set(port "")
    set(libs ${FX_LIBS})
    if(F_PORT)
        target_include_directories(fxo_${name} BEFORE PRIVATE ${BRAAM_COMPAT_INCLUDE_DIR})
        target_link_libraries(fxo_${name} PRIVATE braam::compat braam::portflags)
        set(port PORT NOFLOAT)
        set(libs ${FX_PORT_LIBS})
    endif()

    braam_add_program(NAME fx_${name}
                      SOURCES $<TARGET_OBJECTS:fxo_${name}> $<TARGET_OBJECTS:fx_driver>
                      LIBS ${F_ARCHIVES}
                      ${port})
    set_target_properties(bin_fx_${name} PROPERTIES LINKER_LANGUAGE CXX)

    fx_json_list(objects "$<TARGET_OBJECTS:fxo_${name}>;$<TARGET_OBJECTS:fx_driver>")
    set(files "")
    foreach(a ${F_ARCHIVES})
        list(APPEND files "$<TARGET_FILE:${a}>")
    endforeach()
    fx_json_list(archives "${files}")
    if(NOT F_ARCHIVES)
        set(archives "[]")
    endif()
    set(files "")
    foreach(l ${libs})
        list(APPEND files "$<TARGET_FILE:${l}>")
    endforeach()
    fx_json_list(sdk "${files}")

    string(CONCAT entry
        "  \"${name}\": {\n"
        "    \"objects\": ${objects},\n"
        "    \"archives\": ${archives},\n"
        "    \"libs\": ${sdk},\n"
        "    \"reference\": \"$<TARGET_FILE:bin_fx_${name}>\"\n"
        "  },\n")
    set_property(GLOBAL APPEND_STRING PROPERTY WLINK_FIXTURES "${entry}")
endfunction()

# wlink_bad(<name> <src>...): objects that must not link. No reference is
# made; the tests hold wlink's refusal against wasm-ld's.
function(wlink_bad name)
    add_library(fxo_${name} OBJECT ${ARGN})
    target_link_libraries(fxo_${name} PRIVATE braam::flags)
    target_include_directories(fxo_${name} PRIVATE ${FX_SRC})
    fx_json_list(objects "$<TARGET_OBJECTS:fxo_${name}>;$<TARGET_OBJECTS:fx_driver>")
    set(files "")
    foreach(l ${FX_LIBS})
        list(APPEND files "$<TARGET_FILE:${l}>")
    endforeach()
    fx_json_list(sdk "${files}")
    string(CONCAT entry
        "  \"${name}\": {\n"
        "    \"objects\": ${objects},\n"
        "    \"libs\": ${sdk}\n"
        "  },\n")
    set_property(GLOBAL APPEND_STRING PROPERTY WLINK_BAD "${entry}")
endfunction()

wlink_archive(fxa_one ${FX_SRC}/one_a.c ${FX_SRC}/one_c.c ${FX_SRC}/one_unused.c)
wlink_archive(fxa_two ${FX_SRC}/two_b.c)

wlink_fixture(hello    SOURCES ${FX_SRC}/hello.c)
wlink_fixture(ctors    SOURCES ${FX_SRC}/ctors.c ${FX_SRC}/ctors.cpp)
wlink_fixture(fnptr    SOURCES ${FX_SRC}/fnptr_a.c ${FX_SRC}/fnptr_b.c)
wlink_fixture(weak     SOURCES ${FX_SRC}/weak_a.c ${FX_SRC}/weak_b.c ${FX_SRC}/weak_c.c)
wlink_fixture(undef    SOURCES ${FX_SRC}/undef_a.c ${FX_SRC}/undef_b.c)
wlink_fixture(comdat   SOURCES ${FX_SRC}/comdat_a.cpp ${FX_SRC}/comdat_b.cpp)
wlink_fixture(data     SOURCES ${FX_SRC}/data_a.c ${FX_SRC}/data_b.c)
wlink_fixture(strings  SOURCES ${FX_SRC}/strings_a.c ${FX_SRC}/strings_b.c)
wlink_fixture(export   SOURCES ${FX_SRC}/export.c)
wlink_fixture(imports  SOURCES ${FX_SRC}/imports.c)
wlink_fixture(archives SOURCES ${FX_SRC}/arch_main.c ARCHIVES fxa_one fxa_two)
wlink_fixture(port     SOURCES ${FX_SRC}/port.cpp PORT)

wlink_bad(bad_dup   ${FX_SRC}/bad_dup_a.c ${FX_SRC}/bad_dup_b.c)
wlink_bad(bad_undef ${FX_SRC}/bad_undef_a.c ${FX_SRC}/bad_undef_b.c)
wlink_bad(bad_type  ${FX_SRC}/bad_type_a.c ${FX_SRC}/bad_type_b.c)
wlink_bad(bad_sig   ${FX_SRC}/bad_sig_a.c ${FX_SRC}/bad_sig_b.c)

# llvm-objdump is --dump's oracle, and llvm-ar makes the archive formats the
# SDK's own do not use. Both come from the clang the toolchain found.
get_filename_component(FX_LLVM_BIN ${BRAAM_AR} DIRECTORY)
find_program(WLINK_OBJDUMP NAMES llvm-objdump HINTS ${FX_LLVM_BIN} REQUIRED)

# The trailing "{}" entry keeps the JSON valid after the last comma.
get_property(fixtures GLOBAL PROPERTY WLINK_FIXTURES)
get_property(bad GLOBAL PROPERTY WLINK_BAD)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fixtures.json
     CONTENT "{
  \"wlink\": \"$<TARGET_FILE:bin_wlink>\",
  \"objdump\": \"${WLINK_OBJDUMP}\",
  \"ar\": \"${BRAAM_AR}\",
  \"wasm_ld\": \"${BRAAM_WASM_LD}\",
  \"sdk_libs\": \"$<TARGET_FILE_DIR:braam::proc>\",
  \"fixtures\": {
${fixtures}  \"\": {}
  },
  \"bad\": {
${bad}  \"\": {}
  }
}
")
