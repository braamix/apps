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
# Sources arrive a phase at a time; TODO.md names them.
braam_add_program(NAME python
                  SOURCES astdump.cpp bigint.cpp braam.cpp builtin.cpp bytemeth.cpp bytes.cpp call.cpp
                          code.cpp compile.cpp complex.cpp
                          dis.cpp err.cpp exc.cpp float.cpp format.cpp formatgr.cpp frame.cpp func.cpp gc.cpp
                          gen.cpp import.cpp int.cpp
                          intern.cpp iter.cpp lex.cpp list.cpp mapmeth.cpp method.cpp nummeth.cpp
                          obj.cpp ops.cpp parse.cpp
                          repr.cpp selftest.cpp seqmeth.cpp str.cpp strmeth.cpp symtab.cpp
                          table.cpp tuple.cpp type.cpp
                          vm.cpp
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
