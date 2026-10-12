// vcc's coroutine, defer and alloca run tests, compiled on Braam and run
// (suites.mjs).

import { run_suites } from "./suites.mjs";

const UNITS = "two translation units, the second the test's own";
await run_suites("coro", ["alloca_run_tests", "coro_run_tests", "defer_tests"], false, {
    AllocaLongjmp: "Braam has no setjmp",
    CoroPtrTwoUnits: UNITS,
    CoroTwoUnits: UNITS,
});
