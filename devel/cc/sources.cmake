# The sources of each part of the compiler, for the host build (CMakeLists.txt)
# and the Braam one (braam.cmake).

set(VCC_LIBUTIL
    libutil/c_escape.c libutil/string_map.c libutil/xalloc.c libutil/wio.c
    libutil/float128.c libutil/srcloc.c libutil/memstream.c libutil/readfile.c)
set(VCC_AST
    ast/ast_alloc.c ast/ast_print.c ast/ast_free.c ast/ast_clone.c ast/ast_yaml.c
    ast/ast_export.c ast/ast_import.c ast/ast_compare.c ast/type_to_c.c)
set(VCC_SCANNER scanner/scanner.c)
set(VCC_PARSER
    parser/parser.c parser/expr.c parser/decl.c parser/stmt.c parser/nametab.c)
set(VCC_TAC
    tac/tac_alloc.c tac/tac_print.c tac/tac_free.c tac/tac_compare.c tac/tac_yaml.c
    tac/tac_import.c tac/tac_export.c tac/tac_walk.c tac/tac_verify.c tac/tac_abi.c)
set(VCC_SEMANTIC
    semantic/symtab.c semantic/symtab_print.c semantic/structtab.c
    semantic/structtab_print.c semantic/typetab.c semantic/typetab_print.c
    semantic/typecheck.c semantic/expressions.c semantic/init_normalize.c
    semantic/initializers.c semantic/statements.c semantic/declarations.c
    semantic/type_utils.c semantic/const_convert.c semantic/label_loops.c
    semantic/coroutines.c semantic/defer.c semantic/resolve_labels.c
    semantic/target.c semantic/tac_type.c)
set(VCC_OPTIMIZE
    optimize/optimize.c optimize/const_fold.c optimize/cfg.c optimize/unreachable.c
    optimize/alias.c optimize/dataflow.c optimize/copy_prop.c optimize/cse.c
    optimize/ivsr.c optimize/dead_store.c optimize/liveness.c)
set(VCC_TRANSLATOR
    translator/translate.c translator/expr.c translator/stmt.c translator/coro.c)
set(VCC_BACKEND_COMMON backend/common/driver.c backend/common/flow.c)
set(VCC_WASM
    backend/wasm/wasm_ir.c backend/wasm/emit.c backend/wasm/frame.c
    backend/wasm/instr.c backend/wasm/data.c backend/wasm/structure.c
    backend/wasm/call.c backend/wasm/peephole.c backend/wasm/locals.c
    backend/wasm/codegen.c)
set(VCC_CPP
    cpp/cpp.c cpp/buffer.c cpp/scan.c cpp/direct.c cpp/macro.c cpp/diag.c
    cpp/parser.c cpp/yylex.c cpp/files.c)

# Each program: its main and the parts it links.
set(VCC_PARSE_SOURCES parser/main.c ${VCC_PARSER} ${VCC_SCANNER} ${VCC_AST} ${VCC_LIBUTIL})
set(VCC_LOWER_SOURCES translator/main.c ${VCC_TRANSLATOR} ${VCC_OPTIMIZE} ${VCC_SEMANTIC}
    ${VCC_TAC} ${VCC_AST} ${VCC_LIBUTIL})
set(VCC_GENWASM_SOURCES backend/wasm/main.c ${VCC_WASM} ${VCC_BACKEND_COMMON} ${VCC_TAC}
    ${VCC_LIBUTIL})

set(VCC_CPP_SOURCES ${VCC_CPP} ${VCC_LIBUTIL})

# Every directory a part's headers are in.
set(VCC_INCLUDE_DIRS libutil ast scanner parser tac semantic optimize translator
    backend/common backend/wasm)
