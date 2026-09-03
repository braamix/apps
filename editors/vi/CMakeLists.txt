cmake_minimum_required(VERSION 3.24)

project(vi LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# What braam::portflags does not already carry. An INTERFACE target rather than
# target_compile_options, because braam::flags carries -Wall -Wextra -Wshadow
# and a library's options land after the target's own — so nothing else would
# come last.
add_library(vi_quiet INTERFACE)
target_compile_options(vi_quiet INTERFACE
    -Wno-parentheses -Wno-dangling-else -Wno-char-subscripts)

# ex and vi differ in one bit: which mode they start in. Upstream decided it by
# scanning argv[0], and ex.cpp still does that so a link named view or edit
# works; the define is what makes the two binaries differ when both are called
# by their own name.
# What both binaries are made of. Upstream's file split is kept, and so are
# its file names, except where the file stopped being about what it was named
# for: ex_temp.c became ex_buf.cpp because there is no temp file, ex_put.c
# became ex_out.cpp because only its formatting half survived, and ex_tty.c
# has no successor at all.
set(VI_COMMON
    ex.cpp ex_addr.cpp ex_cmds.cpp ex_cmds2.cpp ex_cmdsub.cpp ex_data.cpp
    ex_get.cpp ex_io.cpp ex_re.cpp ex_set.cpp ex_subr.cpp ex_unix.cpp
    ex_v.cpp ex_vadj.cpp ex_vget.cpp ex_vmain.cpp ex_voper.cpp ex_vops.cpp
    ex_vops2.cpp ex_vops3.cpp ex_vput.cpp ex_vwind.cpp
    ex_buf.cpp ex_out.cpp ex_screen.cpp)

# PORT is the C library ex calls: the string, character and allocation
# routines, and Group B's b_* family, which replaced ex_file.cpp. It carries
# -fno-builtin, which the key-buffer
# loops in ex_vget.cpp need too — int is wchar_t's width here, so clang
# recognises them as wcslen and wcscpy and calls functions that do not exist.
braam_add_program(NAME ex SOURCES ${VI_COMMON} PORT)
braam_add_program(NAME vi SOURCES ${VI_COMMON} PORT)

foreach(t ex vi)
    target_link_libraries(bin_${t} PRIVATE vi_quiet)
endforeach()

target_compile_definitions(bin_vi PRIVATE EX_DEFAULT_VISUAL=1)

# One package, two commands: every flat bin/ entry becomes a cmd: provide and a
# link in the installed generation's bin/, which /pkg/bin points at.
braam_add_package(NAME vi VERSION 3.6-r13
                  FIELD "T=UCB ex/vi 3.6, the visual editor"
                  FILES $<TARGET_FILE:bin_ex>=bin/ex
                        $<TARGET_FILE:bin_vi>=bin/vi)
add_dependencies(packages pkg_vi)
