cmake_minimum_required(VERSION 3.24)

project(python LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# No PORT: this is written in Braam idiom, not recompiled from C, so the port
# kit is neither needed nor wanted. braam::math is float arithmetic and ftoa's
# conversions, which the number tower will want from phase 2 on.
#
# The Unicode tables are 600 KB of initialized data, and wasm-ld wants the
# initial memory to cover them. The cap is still the kernel's.
set(BRAAM_BIN_INITIAL_PAGES 16)
math(EXPR BRAAM_BIN_INITIAL_BYTES "${BRAAM_BIN_INITIAL_PAGES} * 65536")

# Sources arrive a phase at a time; TODO.md names them.
braam_add_program(NAME python
                  SOURCES src/abc.cpp src/annot.cpp src/arraymod.cpp src/astdump.cpp src/astmod.cpp
                          src/astpos.cpp src/atexitmod.cpp src/attr.cpp src/bigint.cpp
                          src/binasciimod.cpp src/binfmt.cpp src/braam.cpp src/builtin.cpp
                          src/bytemeth.cpp src/bytes.cpp src/call.cpp src/code.cpp src/codec.cpp
                          src/codecsmod.cpp src/collmod.cpp src/colorizemod.cpp src/compare.cpp
                          src/compile.cpp src/complex.cpp src/csvmod.cpp src/ctxvars.cpp src/dis.cpp
                          src/dismod.cpp src/edit.cpp src/egroup.cpp src/err.cpp src/exc.cpp src/float.cpp
                          src/flocals.cpp src/format.cpp src/formatgr.cpp src/frame.cpp src/func.cpp
                          src/functoolsmod.cpp src/gc.cpp src/gen.cpp src/genalias.cpp
                          src/hashmod.cpp src/impmod.cpp src/import.cpp src/info.cpp src/int.cpp
                          src/intern.cpp src/iobase.cpp src/iobuf.cpp src/iofile.cpp src/iomem.cpp
                          src/iotext.cpp src/iter.cpp src/itermod.cpp src/lazy.cpp src/lex.cpp
                          src/list.cpp src/mapmeth.cpp src/marshalmod.cpp src/mathmod.cpp
                          src/method.cpp src/miscmod.cpp src/module.cpp src/nummeth.cpp src/obj.cpp
                          src/objmeth.cpp src/opmod.cpp src/ops.cpp src/parse.cpp src/patma.cpp
                          src/posixmod.cpp src/randmod.cpp src/range.cpp src/repr.cpp
                          src/selftest.cpp src/sentinel.cpp src/seqmeth.cpp src/signalmod.cpp
                          src/slotmeth.cpp src/sre.cpp src/sremod.cpp src/str.cpp src/stringmod.cpp
                          src/strmeth.cpp src/structmod.cpp src/symtab.cpp src/sysmod.cpp
                          src/table.cpp src/templatelib.cpp src/thread.cpp src/timemod.cpp
                          src/tokenizemod.cpp src/traceback.cpp src/tuple.cpp src/type.cpp
                          src/typesmod.cpp src/typevar.cpp src/ucd.cpp src/ucddb.cpp src/unimod.cpp
                          src/union.cpp src/vm.cpp src/warnmod.cpp src/weak.cpp
                  LIBS braam::math)

# Floating-point arithmetic is this language's, not the compiler's: contracting
# `a*b - c*d` into an fma is more accurate and therefore wrong, since CPython
# does not and a complex multiply would answer differently.
target_compile_options(bin_python PRIVATE -ffp-contract=off)

# The library ships as lib/, and exactly what lib/manifest.txt lists: a module
# is in the package once it has been taken from CPython and has a row. Editing
# the manifest reconfigures.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             ${CMAKE_CURRENT_SOURCE_DIR}/lib/manifest.txt)
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/lib/manifest.txt PYTHON_LIB_ROWS REGEX "^[^# ]")
set(PYTHON_LIB "")
foreach(row IN LISTS PYTHON_LIB_ROWS)
    string(REGEX MATCH "^[^ ]+" mod "${row}")
    list(APPEND PYTHON_LIB ${CMAKE_CURRENT_SOURCE_DIR}/lib/${mod}=lib/${mod})
endforeach()

# The demos. share/ is nested, so it is payload rather than a command.
# CONFIGURE_DEPENDS because adding one must reconfigure.
file(GLOB PYTHON_DEMOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.py)
set(PYTHON_EXAMPLES "")
foreach(f ${PYTHON_DEMOS})
    get_filename_component(n ${f} NAME)
    list(APPEND PYTHON_EXAMPLES ${f}=share/${n})
endforeach()

braam_add_package(NAME python VERSION 0.1-r0
                  FIELD "T=Python 3, written for Braam"
                  FILES $<TARGET_FILE:bin_python>=bin/python
                        ${PYTHON_LIB}
                        ${PYTHON_EXAMPLES})

if(TARGET packages)
    add_dependencies(packages pkg_python)
endif()
