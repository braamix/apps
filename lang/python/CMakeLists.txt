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
                  SOURCES abc.cpp annot.cpp arraymod.cpp astdump.cpp astmod.cpp astpos.cpp atexitmod.cpp attr.cpp bigint.cpp binasciimod.cpp binfmt.cpp braam.cpp builtin.cpp bytemeth.cpp bytes.cpp call.cpp codec.cpp codecsmod.cpp
                          code.cpp collmod.cpp compare.cpp compile.cpp csvmod.cpp ctxvars.cpp complex.cpp
                          dis.cpp dismod.cpp egroup.cpp err.cpp exc.cpp float.cpp flocals.cpp format.cpp formatgr.cpp frame.cpp func.cpp functoolsmod.cpp gc.cpp
                          gen.cpp genalias.cpp hashmod.cpp import.cpp info.cpp int.cpp iobase.cpp iobuf.cpp iofile.cpp iomem.cpp iotext.cpp
                          intern.cpp iter.cpp itermod.cpp lazy.cpp lex.cpp marshalmod.cpp impmod.cpp colorizemod.cpp list.cpp mapmeth.cpp method.cpp nummeth.cpp
                          mathmod.cpp miscmod.cpp module.cpp posixmod.cpp obj.cpp objmeth.cpp opmod.cpp ops.cpp parse.cpp patma.cpp
                          randmod.cpp range.cpp repr.cpp selftest.cpp sentinel.cpp signalmod.cpp seqmeth.cpp slotmeth.cpp sre.cpp sremod.cpp str.cpp stringmod.cpp strmeth.cpp structmod.cpp symtab.cpp
                          sysmod.cpp table.cpp templatelib.cpp thread.cpp timemod.cpp tokenizemod.cpp traceback.cpp tuple.cpp type.cpp typesmod.cpp typevar.cpp ucd.cpp ucddb.cpp unimod.cpp union.cpp weak.cpp
                          vm.cpp warnmod.cpp
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

braam_add_package(NAME python VERSION 0.1-r0
                  FIELD "T=Python 3, written for Braam"
                  FILES $<TARGET_FILE:bin_python>=bin/python
                        ${PYTHON_LIB})

if(TARGET packages)
    add_dependencies(packages pkg_python)
endif()
