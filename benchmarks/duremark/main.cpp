/*
 * DureMark Main Benchmark Execution
 * Orchestrates workload execution and calculates scores
 *
 * Braam: main is a coroutine, because writing is one, and the timer wraps
 * each workload's whole loop rather than each call. See README.md.
 */
#include "duremark.h"

/* Run behckmark for at least so long. */
enum {
    TARGET_SECONDS = 3,
};

/* Function: du_iterate
 * Run benchmark iterations
 *
 * noinline: du_main is a coroutine, and inlining this into it would move the
 * workload's state into a heap-allocated frame.
 */
static __attribute__((noinline)) void du_iterate(du_results *res)
{
    uint32_t i;

    for (i = 0; i < res->iterations; i++) {
        /* List workload */
        if (res->execs & ID_LIST) {
            du_bench_list(res, (int16_t)(i & 0x7FFF));
        }

        /* Matrix workload */
        if (res->execs & ID_MATRIX) {
            du_bench_matrix(&res->mat, (int16_t)(i & 0x7FFF));
        }

        /* State machine workload */
        if (res->execs & ID_STATE) {
            du_bench_state(i & 0x7F);
        }
    }
}

/* Function: du_time_one
 * Run one workload over the whole loop, timed.
 *
 * The original started and stopped the clock around each single workload
 * call. This clock counts whole milliseconds, so every one of those intervals
 * measured zero and the benchmark never converged; timing the loop instead is
 * what the execs mask is for.
 */
static du_ticks_t du_time_one(du_results *res, unsigned which)
{
    res->execs = which;
    du_start_time();
    du_iterate(res);
    return du_get_time();
}

/* Function: du_main
 * Main entry point
 */
Task<i32> du_main(void)
{
    du_results res;
    double sec_per_tick = du_get_sec_per_tick();
    double total_sec;
    int multiplier_count;

    /* Initialize results */
    res.list         = NULL;
    res.err          = 0;
    res.list_ticks   = 0;
    res.matrix_ticks = 0;
    res.state_ticks  = 0;
    res.total_ticks  = 0;

    /* Set which algorithms to run */
    res.execs = ID_LIST | ID_MATRIX | ID_STATE;

    /* ^C stops the ladder rather than the process, so a run cut short still
       reports the pass that finished. */
    int interrupted = 0;
    if (Task<Result<void>> t = sig_catch(SIG_INT))
        co_await t;

    /* Initialize workloads */
    res.list = du_list_init();
    if (!res.list) {
        du_printf("ERROR: Cannot allocate lists!\n");
        co_return 1;
    }
    du_init_matrix(&res.mat);
    du_init_state();

    /* Start with three iterations */
    res.iterations = 3;

    /* Run benchmark with auto-scaling iterations */
    for (multiplier_count = 0; multiplier_count < 20; multiplier_count++) {
        du_printf("Try %lu iterations...\n", res.iterations);
        co_await du_flush();

        /* Run benchmark, one timed pass per workload */
        res.list_ticks   = du_time_one(&res, ID_LIST);
        res.matrix_ticks = du_time_one(&res, ID_MATRIX);
        res.state_ticks  = du_time_one(&res, ID_STATE);
        res.total_ticks  = res.list_ticks + res.matrix_ticks + res.state_ticks;

        /* A workload parks nowhere, so this is where a ^C from the middle of
           one arrives. Between the timed passes, so it measures nothing. */
        if (Task<Result<void>> t = sleep_for(0))
            co_await t;
        if (sig_take(SIG_INT)) {
            interrupted = 1;
            break;
        }

        /* Check if we need to increase iterations. The cast is C++20's: it
           deprecates comparing a double with an unscoped enumeration. */
        if (res.total_ticks * sec_per_tick >= (double)TARGET_SECONDS) {
            /* Execution time >= target, we're done */
            break;
        }

        /* Increase iterations by 3x or 10/3x */
        if (res.iterations % 3 == 0) {
            res.iterations = res.iterations * 10 / 3;
        } else {
            res.iterations *= 3;
        }
    }

    /* Output results */
    if (interrupted) {
        du_printf("\nInterrupted: the results below are the last pass that finished.\n");
        if (res.total_ticks == 0) {
            /* Nothing was measured, and every line below would divide by it. */
            co_await du_flush();
            co_return 130;
        }
    }
    total_sec = res.total_ticks * sec_per_tick;
    du_printf("\n");
    du_printf("DureMark 1.1 Results\n");
    du_printf("=======================\n");
    du_printf("Iterations      : %lu\n", res.iterations);
    du_printf("Execution Time  : %.1f sec\n", total_sec);
    du_printf("List Workload   : %.1f%%\n", res.list_ticks * 100.0 / res.total_ticks);
    du_printf("Matrix Workload : %.1f%%\n", res.matrix_ticks * 100.0 / res.total_ticks);
    du_printf("State Workload  : %.1f%%\n", res.state_ticks * 100.0 / res.total_ticks);
    du_printf("-----------------------\n");
    du_printf("Total Score     : %.2f DureMark\n\n", res.iterations / total_sec);
    co_await du_flush();
    co_return interrupted ? 130 : 0;
}
