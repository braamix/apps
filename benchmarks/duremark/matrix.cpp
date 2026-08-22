/*
 * DureMark Matrix Operations Workload
 * Simplified matrix operations with small fixed-size matrices
 */
#include "duremark.h"

/*
 * Define matrix size
 */
#define N 10

/*
 * Memory needed: N*N*8 bytes (A: 2 bytes/elem, B: 2 bytes/elem, C: 4 bytes/elem)
 *
 * alignas: the block is cast to matdat_t* and matres_t*, and a uint8_t array
 * is free to be byte-aligned.
 */
alignas(matres_t) static uint8_t mem_matrix[N * N * 8];

/* Function: du_matrix_add_const
 * Add a constant to all elements of matrix A
 */
static void du_matrix_add_const(matdat_t *A, matdat_t val)
{
    uint16_t i;
    uint16_t j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            A[i * N + j] += val;
        }
    }
}

/* Function: du_matrix_mul_const
 * Multiply matrix A by a constant, store in C
 */
static void du_matrix_mul_const(matres_t *C, matdat_t *A, matdat_t val)
{
    uint16_t i;
    uint16_t j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            C[i * N + j] = (matres_t)A[i * N + j] * (matres_t)val;
        }
    }
}

/* Function: du_matrix_mul
 * Multiply matrix A by matrix B, store in C
 */
static void du_matrix_mul(matres_t *C, matdat_t *A, matdat_t *B)
{
    uint16_t i;
    uint16_t j;
    uint16_t k;

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            C[i * N + j] = 0;
            for (k = 0; k < N; k++) {
                C[i * N + j] += (matres_t)A[i * N + k] * (matres_t)B[k * N + j];
            }
        }
    }
}

/* Function: du_matrix_sum
 * Sum all elements of matrix C
 */
static int32_t du_matrix_sum(matres_t *C)
{
    uint16_t i;
    uint16_t j;
    int32_t sum;

    sum = 0;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            sum += C[i * N + j];
        }
    }
    return sum;
}

/* Function: du_init_matrix
 * Initialize matrices from memory block
 */
uint16_t du_init_matrix(mat_params *p)
{
    matdat_t *A;
    matdat_t *B;
    matdat_t val;
    uint16_t i;
    uint16_t j;
    int32_t seed  = 1;
    int32_t order = 1;

    A = (matdat_t *)mem_matrix;
    B = A + N * N;

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            seed = ((order * seed) % 65536);
            val  = (matdat_t)(seed + order);
            if (val > 1000) {
                val = val % 1000;
            }
            B[i * N + j] = val;
            val          = (matdat_t)(val + order);
            if (val > 1000) {
                val = val % 1000;
            }
            A[i * N + j] = val;
            order++;
        }
    }

    p->A = A;
    p->B = B;
    p->C = (matres_t *)(B + N * N);

    return N;
}

/* Function: du_bench_matrix
 * Benchmark matrix operations
 */
void du_bench_matrix(mat_params *p, matdat_t val)
{
    matres_t *C;
    matdat_t *A;
    matdat_t *B;

    C = p->C;
    A = p->A;
    B = p->B;
    if (val == 0) {
        val = 1;
    }

    /* Add constant to A */
    du_matrix_add_const(A, val);

    /* Multiply A by constant */
    du_matrix_mul_const(C, A, val);
    du_matrix_sum(C);

    /* Multiply A by B */
    du_matrix_mul(C, A, B);
    du_matrix_sum(C);

    /* Restore A */
    du_matrix_add_const(A, (matdat_t)(-val));
}
