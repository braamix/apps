//
// genwasm: the WebAssembly code generator, on the shared backend driver.
//
#include <stddef.h>

#include "codegen.h"
#include "driver.h"

static const BackendFlag flags[] = {
    { "no-structure", "the dispatch skeleton for every function, no blocks and loops" },
    { "no-peephole", "no peephole rewrites of the finished code" },
    { "no-stackify", "every value through a local, none left on the operand stack" },
    { "no-coalesce", "a local for every name, none shared" },
    { "no-regional",
      "the dispatch skeleton for an irreducible function, not a dispatch per region" },
    { NULL, NULL },
};

static void flag(int index)
{
    switch (index) {
    case 0:
        wasm_structure = false;
        break;
    case 1:
        wasm_peephole = false;
        break;
    case 2:
        wasm_stackify = false;
        break;
    case 3:
        wasm_coalesce = false;
        break;
    case 4:
        wasm_regional = false;
        break;
    }
}

static const char *output_ext(void)
{
    return ".s";
}

int main(int argc, char *argv[])
{
    static const Backend wasm = { flags, flag, output_ext, wasm_codegen };
    return backend_main(argc, argv, &wasm);
}
