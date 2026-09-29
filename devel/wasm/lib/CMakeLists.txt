cmake_minimum_required(VERSION 3.24)

project(wasmobj LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# What reads and writes wasm modules, objects and archives, for every tool in
# the package, and LLVM's C++ name demangler. Pure: no co_await, no files.
add_library(wasmobj STATIC archive.cpp cursor.cpp emit.cpp files.cpp module.cpp object.cpp optable.cpp stamp.cpp symbols.cpp wasm.cpp
                           demangle/demangle.cpp)
# The demangler's loops must not become calls to strlen and memcpy.
set_source_files_properties(demangle/demangle.cpp PROPERTIES COMPILE_OPTIONS -fno-builtin)
target_link_libraries(wasmobj PRIVATE braam::flags)
target_include_directories(wasmobj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
