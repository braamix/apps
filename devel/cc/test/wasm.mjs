// The wasm32 backend's run tests, compiled on Braam and run (suites.mjs).

import { run_suites } from "./suites.mjs";

const ERRNO = "Braam's errno numbers are not wasm32's";
await run_suites("wasm", ["call_tests", "data_tests", "float128_tests", "flow_tests",
                          "frame_tests", "int_tests", "interop_tests", "math_tests",
                          "mem_tests", "peephole_tests", "printf_tests", "ptr_tests",
                          "str_tests", "struct_tests"], false, {
    SqrtLibrary: "calls the test's own croot",
    StrerrorKnown: ERRNO,
    StrerrorUnknown: ERRNO,
});
