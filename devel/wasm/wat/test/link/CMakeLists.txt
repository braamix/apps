# The C half of test/link.mjs, and link.json naming it, the driver of ld's
# fixtures and the SDK's archives, for the test to link.
add_library(wat_link_c OBJECT link.c)
target_link_libraries(wat_link_c PRIVATE braam::flags)
target_include_directories(wat_link_c PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../ld/test/src)

fx_json_list(objects "$<TARGET_OBJECTS:wat_link_c>;$<TARGET_OBJECTS:fx_driver>")
fx_json_list(libs "$<TARGET_FILE:braam::proc>;$<TARGET_FILE:braam::ui>")
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/link.json
     CONTENT "{ \"objects\": ${objects}, \"libs\": ${libs} }\n")
