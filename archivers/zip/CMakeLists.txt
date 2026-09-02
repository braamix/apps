cmake_minimum_required(VERSION 3.24)

project(zip LANGUAGES CXX)

# On its own this finds the SDK; under the tree's own CMakeLists the targets
# are already defined and this file is one more add_subdirectory.
if(NOT TARGET braam::proc)
    find_package(braam REQUIRED)
endif()

# What every one of the four is made of. Upstream compiled these a second time
# with -DUTIL for the three tools, which strips the create path out of them;
# here they are compiled once and --gc-sections keeps out of each binary what
# that binary never reaches.
set(ZIP_COMMON zipfile.cpp fileio.cpp util.cpp globals.cpp crc32.cpp
               crypt.cpp ttyio.cpp unix.cpp braam.cpp)

# PORT is the C library: the mem*, str*, ctype, allocation, strtol and qsort
# names upstream calls, Group B's streams as the b_* family, and -fno-builtin
# for all of them at once. What braam.cpp still answers is what the kit has not
# got -- two date formats, the DOS stamp, and a read a ^C has to reach.
braam_add_program(NAME zip
                  SOURCES zip.cpp zipup.cpp deflate.cpp trees.cpp ${ZIP_COMMON}
                  PORT NOFLOAT)
braam_add_program(NAME zipnote   SOURCES zipnote.cpp ${ZIP_COMMON} PORT NOFLOAT)
braam_add_program(NAME zipsplit  SOURCES zipsplit.cpp ${ZIP_COMMON} PORT NOFLOAT)
braam_add_program(NAME zipcloak  SOURCES zipcloak.cpp ${ZIP_COMMON} PORT NOFLOAT)

# One package, four commands: every flat bin/ entry becomes a cmd: provide and
# a link in the installed generation's bin/, which /pkg/bin points at.
braam_add_package(NAME zip VERSION 3.0-r3
                  FIELD "T=Info-ZIP zip 3.0, the archiver"
                  FILES $<TARGET_FILE:bin_zip>=bin/zip
                        $<TARGET_FILE:bin_zipnote>=bin/zipnote
                        $<TARGET_FILE:bin_zipsplit>=bin/zipsplit
                        $<TARGET_FILE:bin_zipcloak>=bin/zipcloak)
add_dependencies(packages pkg_zip)
