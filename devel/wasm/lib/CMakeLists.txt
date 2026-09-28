cmake_minimum_required(VERSION 3.24)

project(wasmobj LANGUAGES CXX)

if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# What reads and writes wasm modules, objects and archives, for every tool in
# the package. Pure: no co_await, no files.
add_library(wasmobj STATIC archive.cpp cursor.cpp emit.cpp module.cpp object.cpp wasm.cpp)
target_link_libraries(wasmobj PRIVATE braam::flags)
target_include_directories(wasmobj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
