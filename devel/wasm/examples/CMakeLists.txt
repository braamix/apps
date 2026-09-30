# libw.a, for the package's lib/: the examples' own sources, assembled
# by as and archived by ar, under the SDK's harness.
find_program(NODE node REQUIRED)
get_filename_component(BRAAM_PREFIX ${braam_DIR}/../../.. ABSOLUTE)

add_custom_command(OUTPUT libw.a
                   COMMAND ${CMAKE_COMMAND} -E env BRAAM_SDK=${BRAAM_PREFIX}
                           ${NODE} ${CMAKE_CURRENT_SOURCE_DIR}/mklib.mjs
                           $<TARGET_FILE:bin_as> $<TARGET_FILE:bin_ar> ${CMAKE_CURRENT_BINARY_DIR}
                   DEPENDS bin_as bin_ar mklib.mjs proc.s fmt.s args.s ../as/host.mjs
                   COMMENT "Building libw.a with as and ar")
add_custom_target(wasm_lib ALL DEPENDS libw.a)
