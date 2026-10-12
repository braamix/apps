cmake_minimum_required(VERSION 3.24)

# The compiler on the host, built by the host's C compiler: the cross compiler
# that builds this package (Plan.md, decision 1). Configure it without the
# SDK's toolchain file, in a build directory of its own.
project(cc LANGUAGES C)

# Apple's linker warns at a static library named twice.
cmake_policy(SET CMP0156 NEW)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wshadow")

add_library(libutil STATIC
    libutil/c_escape.c libutil/string_map.c libutil/xalloc.c libutil/wio.c
    libutil/float128.c libutil/srcloc.c)
target_include_directories(libutil PUBLIC libutil)
target_link_libraries(libutil m)

add_library(ast STATIC
    ast/ast_alloc.c ast/ast_print.c ast/ast_free.c ast/ast_clone.c ast/ast_yaml.c
    ast/ast_export.c ast/ast_import.c ast/ast_compare.c
    ast/type_to_c.c)
target_include_directories(ast PUBLIC ast)
target_link_libraries(ast libutil)

add_library(scanner STATIC scanner/scanner.c)
target_include_directories(scanner PUBLIC scanner)
target_link_libraries(scanner PUBLIC libutil)

add_library(parser STATIC
    parser/parser.c parser/expr.c parser/decl.c parser/stmt.c parser/nametab.c)
target_include_directories(parser PUBLIC parser)
target_link_libraries(parser scanner ast libutil)

add_library(tac STATIC
    tac/tac_alloc.c tac/tac_print.c tac/tac_free.c tac/tac_compare.c tac/tac_yaml.c
    tac/tac_import.c tac/tac_export.c tac/tac_walk.c
    tac/tac_verify.c tac/tac_abi.c)
target_include_directories(tac PUBLIC tac)
target_link_libraries(tac libutil)

add_library(semantic STATIC
    semantic/symtab.c semantic/symtab_print.c semantic/structtab.c
    semantic/structtab_print.c semantic/typetab.c semantic/typetab_print.c
    semantic/typecheck.c semantic/expressions.c semantic/init_normalize.c
    semantic/initializers.c semantic/statements.c semantic/declarations.c
    semantic/type_utils.c semantic/const_convert.c semantic/label_loops.c
    semantic/coroutines.c semantic/defer.c semantic/resolve_labels.c
    semantic/target.c semantic/tac_type.c)
target_include_directories(semantic PUBLIC semantic)
target_link_libraries(semantic tac ast libutil)

add_library(optimize STATIC
    optimize/optimize.c optimize/const_fold.c optimize/cfg.c optimize/unreachable.c
    optimize/alias.c optimize/dataflow.c optimize/copy_prop.c optimize/cse.c
    optimize/ivsr.c optimize/dead_store.c optimize/liveness.c)
target_include_directories(optimize PUBLIC optimize)
target_link_libraries(optimize tac libutil semantic)

add_library(translator STATIC
    translator/translate.c translator/expr.c translator/stmt.c translator/coro.c)
target_include_directories(translator PUBLIC translator)
target_link_libraries(translator optimize semantic tac ast libutil)

add_library(backend_common STATIC
    backend/common/driver.c backend/common/flow.c)
target_include_directories(backend_common PUBLIC backend/common)
target_link_libraries(backend_common tac libutil)

add_library(wasm STATIC
    backend/wasm/wasm_ir.c backend/wasm/emit.c backend/wasm/frame.c
    backend/wasm/instr.c backend/wasm/data.c backend/wasm/structure.c
    backend/wasm/call.c backend/wasm/peephole.c backend/wasm/locals.c
    backend/wasm/codegen.c)
target_include_directories(wasm PUBLIC backend/wasm)
target_link_libraries(wasm backend_common tac libutil)

add_executable(cpp
    cpp/cpp.c cpp/buffer.c cpp/scan.c cpp/direct.c cpp/macro.c cpp/diag.c
    cpp/parser.c cpp/yylex.c)
target_include_directories(cpp PRIVATE cpp)
target_compile_options(cpp PRIVATE -fno-common)
target_compile_definitions(cpp PRIVATE VCC_SHARE_DIR="${CMAKE_BINARY_DIR}/share/vcc")

add_executable(parse parser/main.c)
target_link_libraries(parse parser scanner)

add_executable(lower translator/main.c)
target_link_libraries(lower optimize translator)

add_executable(genwasm backend/wasm/main.c)
target_link_libraries(genwasm wasm backend_common)

add_executable(cc cc/cc.c)
