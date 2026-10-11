# libw.a, for the package's lib/: the examples' own sources, assembled
# by wat and archived by ar, under the SDK's harness.
find_program(NODE node REQUIRED)
get_filename_component(BRAAM_PREFIX ${braam_DIR}/../../.. ABSOLUTE)

add_custom_command(OUTPUT libw.a
                   COMMAND ${CMAKE_COMMAND} -E env BRAAM_SDK=${BRAAM_PREFIX}
                           ${NODE} ${CMAKE_CURRENT_SOURCE_DIR}/mklib.mjs
                           $<TARGET_FILE:bin_wat> $<TARGET_FILE:bin_ar> ${CMAKE_CURRENT_BINARY_DIR}
                   DEPENDS bin_wat bin_ar mklib.mjs proc.wat fmt.wat args.wat ../wat/host.mjs
                   COMMENT "Building libw.a with wat and ar")
add_custom_target(wasm_lib ALL DEPENDS libw.a)
