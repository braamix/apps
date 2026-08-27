cmake_minimum_required(VERSION 3.24)

project(zip LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# braam.cpp supplies the C library. Each name needs -fno-builtin, or clang
# folds a call into inline code, or turns the definition into a call to itself.
set(ZIP_NOBUILTIN
    -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
    -fno-builtin-memcpy -fno-builtin-memmove -fno-builtin-memset -fno-builtin-memcmp
    -fno-builtin-strlen -fno-builtin-strcpy -fno-builtin-strncpy -fno-builtin-strcat
    -fno-builtin-strcmp -fno-builtin-strncmp -fno-builtin-strchr -fno-builtin-strrchr
    -fno-builtin-strstr -fno-builtin-strncat
    # clang turns upstream's `while (wide[n]) n++` into a wcslen call, and
    # there is no wcslen to call.
    -fno-builtin-wcslen -fno-builtin-toupper -fno-builtin-tolower
    -fno-builtin-isspace -fno-builtin-isdigit -fno-builtin-isalpha -fno-builtin-isalnum -fno-builtin-isprint -fno-builtin-iscntrl -fno-builtin-getenv -fno-builtin-atoi -fno-builtin-strtol -fno-builtin-qsort)

# Ported code keeps upstream's identifiers, its casts and its unused arguments;
# -Wall -Wextra -Wshadow is the tree's, and these fire all over 1990s C that is
# doing nothing wrong. An INTERFACE target rather than target_compile_options,
# because braam::flags carries the -W flags and a library's options land after
# the target's own — so nothing else would come last.
add_library(zip_quiet INTERFACE)
target_compile_options(zip_quiet INTERFACE
    -Wno-shadow -Wno-sign-compare -Wno-unused-parameter
    -Wno-writable-strings -Wno-unused-but-set-variable)

# What every one of the four is made of. Upstream compiled these a second time
# with -DUTIL for the three tools, which strips the create path out of them;
# here they are compiled once and --gc-sections keeps out of each binary what
# that binary never reaches.
set(ZIP_COMMON zipfile.cpp fileio.cpp util.cpp globals.cpp crc32.cpp
               crypt.cpp ttyio.cpp unix.cpp braam.cpp)

braam_add_program(NAME zip
                  SOURCES zip.cpp zipup.cpp deflate.cpp trees.cpp ${ZIP_COMMON})
braam_add_program(NAME zipnote   SOURCES zipnote.cpp ${ZIP_COMMON})
braam_add_program(NAME zipsplit  SOURCES zipsplit.cpp ${ZIP_COMMON})
braam_add_program(NAME zipcloak  SOURCES zipcloak.cpp ${ZIP_COMMON})

foreach(t zip zipnote zipsplit zipcloak)
    target_compile_options(bin_${t} PRIVATE ${ZIP_NOBUILTIN})
    target_link_libraries(bin_${t} PRIVATE zip_quiet)
endforeach()

# One package, four commands: every flat bin/ entry becomes a cmd: provide and
# a link in the installed generation's bin/, which /pkg/bin points at.
braam_add_package(NAME zip VERSION 3.0-r1
                  FIELD "T=Info-ZIP zip 3.0, the archiver"
                  FILES $<TARGET_FILE:bin_zip>=bin/zip
                        $<TARGET_FILE:bin_zipnote>=bin/zipnote
                        $<TARGET_FILE:bin_zipsplit>=bin/zipsplit
                        $<TARGET_FILE:bin_zipcloak>=bin/zipcloak)
add_dependencies(packages pkg_zip)
