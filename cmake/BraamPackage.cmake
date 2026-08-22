# braam_add_package for an SDK that has none.
#
# The SDK ships the function and mkpkg.py from the release after 0.4.162. This
# file is what stands in until then: the same interface, driving mkpkg.py from
# the core tree beside this checkout. Delete it, and the block in
# CMakeLists.txt that includes it, once the pinned SDK carries its own.

if(NOT BRAAM_MKPKG)
    message(FATAL_ERROR "BraamPackage.cmake: BRAAM_MKPKG names nothing")
endif()

function(braam_add_package)
    cmake_parse_arguments(P "" "NAME;VERSION" "FIELD;FILES" ${ARGN})
    if("${P_NAME}" STREQUAL "" OR "${P_VERSION}" STREQUAL "" OR
       "${P_FILES}" STREQUAL "")
        message(FATAL_ERROR "braam_add_package: NAME, VERSION and FILES are required")
    endif()

    set(zip ${CMAKE_CURRENT_BINARY_DIR}/${P_NAME}-${P_VERSION}.zip)

    # A version bump leaves the old zip in the build tree, and `make index`
    # would publish a package the tree can no longer build. Configure time is
    # the moment to drop it: a bump edits this call, which reconfigures.
    file(GLOB stale ${CMAKE_CURRENT_BINARY_DIR}/${P_NAME}-*.zip)
    foreach(f IN LISTS stale)
        if(NOT f STREQUAL zip)
            file(REMOVE ${f})
        endif()
    endforeach()

    set(fields "")
    foreach(f IN LISTS P_FIELD)
        if(NOT f MATCHES "^[A-Za-z]=")
            message(FATAL_ERROR
                "braam_add_package: FIELD ${f} is not <letter>=<value>"
                " (quote a value that has a space in it)")
        endif()
        list(APPEND fields --field ${f})
    endforeach()

    set(sources "")
    foreach(pair IN LISTS P_FILES)
        string(FIND "${pair}" "=" at)
        if(at EQUAL -1)
            message(FATAL_ERROR "braam_add_package: ${pair} is not <src>=<entry>")
        endif()
        string(SUBSTRING "${pair}" 0 ${at} src)
        list(APPEND sources ${src})
    endforeach()

    add_custom_command(OUTPUT ${zip}
        COMMAND ${Python3_EXECUTABLE} ${BRAAM_MKPKG}
                --out ${zip} --name ${P_NAME} --version ${P_VERSION}
                ${fields} ${P_FILES}
        DEPENDS ${sources}
        COMMENT "Packing ${P_NAME}-${P_VERSION}.zip"
        VERBATIM)
    add_custom_target(pkg_${P_NAME} DEPENDS ${zip})
endfunction()
