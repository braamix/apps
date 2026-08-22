/*
 * DureMark Benchmark - Main Header
 * Copyright (c) 2025 Serge Vakulenko
 *
 * Simplified CPU benchmark inspired by CoreMark
 * Designed for 8-bit, 16-bit, and 32-bit processors
 */
#ifndef DUREMARK_H
#define DUREMARK_H

/*
 * Include standard integer types
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdint.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <stdint.h>
#else
/* Manual definitions for older compilers */

/* 8-bit types */
typedef unsigned char uint8_t;
typedef signed char int8_t;

/* 16-bit types */
typedef unsigned short uint16_t;
typedef signed short int16_t;

/* 32-bit types - use long which is typically 32-bit on most systems */
/* Note: On 16-bit systems, long may be 32-bit while int is 16-bit */
typedef unsigned long uint32_t;
typedef signed long int32_t;
#endif

/*
 * Include NULL definition
 */
#include <stddef.h>

/* Forward declaration for timing type */
typedef uint32_t du_ticks_t;

/* Include porting layer */
#if defined(__wasm__)
#include "braam.h"
#elif defined(MSDOS) || defined(__MSDOS__)
#include "dos.h"
#else
#include "unix.h"
#endif

/* Algorithm IDs */
enum {
    ID_LIST   = 1 << 0,
    ID_MATRIX = 1 << 1,
    ID_STATE  = 1 << 2,
};

/* List data structures */
typedef struct du_list_node {
    struct du_list_node *next;
    uint16_t data;
    uint16_t idx;
} du_list_node;

/* Matrix types */
typedef int16_t matdat_t;
typedef int32_t matres_t;

typedef struct mat_params {
    matdat_t *A;
    matdat_t *B;
    matres_t *C;
} mat_params;

/* State machine states */
typedef enum {
    DU_STATE_START = 0,
    DU_STATE_DIGIT,
    DU_STATE_DOT,
    DU_STATE_SIGN,
    DU_STATE_INVALID,
    NUM_STATES
} du_state_t;

/* Results structure */
typedef struct {
    /* Inputs */
    unsigned long iterations;
    unsigned execs;

    /* Workload-specific data */
    du_list_node *list;
    mat_params mat;

    /* Outputs */
    int16_t err;

    /* Timing (in ticks) */
    du_ticks_t list_ticks;
    du_ticks_t matrix_ticks;
    du_ticks_t state_ticks;
    du_ticks_t total_ticks;
} du_results;

/* Function declarations */
/* List functions */
du_list_node *du_list_init(void);
void du_bench_list(du_results *res, int16_t finder_idx);

/* Matrix functions */
uint16_t du_init_matrix(mat_params *p);
void du_bench_matrix(mat_params *p, matdat_t val);

/* State machine functions */
void du_init_state(void);
void du_bench_state(int16_t step);

#endif /* DUREMARK_H */
