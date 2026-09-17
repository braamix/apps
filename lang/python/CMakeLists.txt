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
                  SOURCES abc.cpp arraymod.cpp astdump.cpp atexitmod.cpp attr.cpp bigint.cpp binfmt.cpp braam.cpp builtin.cpp bytemeth.cpp bytes.cpp call.cpp codec.cpp codecsmod.cpp
                          code.cpp collmod.cpp compare.cpp compile.cpp ctxvars.cpp complex.cpp
                          dis.cpp egroup.cpp err.cpp exc.cpp float.cpp flocals.cpp format.cpp formatgr.cpp frame.cpp func.cpp functoolsmod.cpp gc.cpp
                          gen.cpp genalias.cpp import.cpp info.cpp int.cpp
                          intern.cpp iter.cpp itermod.cpp lazy.cpp lex.cpp list.cpp mapmeth.cpp method.cpp nummeth.cpp
                          mathmod.cpp miscmod.cpp module.cpp obj.cpp objmeth.cpp opmod.cpp ops.cpp parse.cpp patma.cpp
                          randmod.cpp range.cpp repr.cpp selftest.cpp sentinel.cpp seqmeth.cpp slotmeth.cpp str.cpp stringmod.cpp strmeth.cpp structmod.cpp symtab.cpp
                          sysmod.cpp table.cpp templatelib.cpp thread.cpp tuple.cpp type.cpp typesmod.cpp typevar.cpp ucd.cpp ucddb.cpp unimod.cpp union.cpp weak.cpp
                          vm.cpp warnmod.cpp
                  LIBS braam::math)

# Floating-point arithmetic is this language's, not the compiler's: contracting
# `a*b - c*d` into an fma is more accurate and therefore wrong, since CPython
# does not and a complex multiply would answer differently.
target_compile_options(bin_python PRIVATE -ffp-contract=off)

braam_add_package(NAME python VERSION 0.1-r0
                  FIELD "T=Python 3, written for Braam"
                  FILES $<TARGET_FILE:bin_python>=bin/python)

if(TARGET packages)
    add_dependencies(packages pkg_python)
endif()
