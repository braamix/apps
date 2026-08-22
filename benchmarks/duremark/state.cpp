/*
 * DureMark State Machine Workload
 * Simplified state machine to detect numbers (integer or float)
 */
#include "duremark.h"

static uint8_t mem_state[400];

/* Function: du_isdigit
 * Check if character is a digit
 */
static uint8_t du_isdigit(uint8_t c)
{
    if (c >= '0' && c <= '9') {
        return 1;
    }
    return 0;
}

/* Function: du_state_transition
 * Process state machine transition
 */
static du_state_t du_state_transition(uint8_t **instr, uint32_t *transition_count)
{
    uint8_t *str;
    uint8_t next_symbol;
    du_state_t state;

    str   = *instr;
    state = DU_STATE_START;

    while (*str != 0 && *str != ',' && state != DU_STATE_INVALID) {
        next_symbol = *str;

        switch (state) {
        case DU_STATE_START:
            if (du_isdigit(next_symbol)) {
                state = DU_STATE_DIGIT;
                transition_count[DU_STATE_START]++;
            } else if (next_symbol == '+' || next_symbol == '-') {
                state = DU_STATE_SIGN;
                transition_count[DU_STATE_START]++;
            } else if (next_symbol == '.') {
                state = DU_STATE_DOT;
                transition_count[DU_STATE_START]++;
            } else {
                state = DU_STATE_INVALID;
                transition_count[DU_STATE_START]++;
            }
            break;

        case DU_STATE_SIGN:
            if (du_isdigit(next_symbol)) {
                state = DU_STATE_DIGIT;
                transition_count[DU_STATE_SIGN]++;
            } else if (next_symbol == '.') {
                state = DU_STATE_DOT;
                transition_count[DU_STATE_SIGN]++;
            } else {
                state = DU_STATE_INVALID;
                transition_count[DU_STATE_SIGN]++;
            }
            break;

        case DU_STATE_DIGIT:
            if (next_symbol == '.') {
                state = DU_STATE_DOT;
                transition_count[DU_STATE_DIGIT]++;
            } else if (!du_isdigit(next_symbol)) {
                state = DU_STATE_INVALID;
                transition_count[DU_STATE_DIGIT]++;
            }
            break;

        case DU_STATE_DOT:
            if (!du_isdigit(next_symbol)) {
                state = DU_STATE_INVALID;
                transition_count[DU_STATE_DOT]++;
            }
            break;

        case DU_STATE_INVALID:
            break;
        case NUM_STATES:
            /* Should never reach here */
            break;
        }

        str++;
    }

    if (*str == ',') {
        str++;
    }

    *instr = str;
    return state;
}

/* Function: du_init_state
 * Initialize state machine input data
 */
void du_init_state()
{
    int16_t seed  = 0x66;
    uint8_t *p    = mem_state;
    uint32_t size = sizeof(mem_state);
    uint32_t total;
    uint32_t next;
    uint32_t i;
    uint8_t *patterns[] = { (uint8_t *)"123",  (uint8_t *)"45.6", (uint8_t *)"-78",
                            (uint8_t *)"+9.0", (uint8_t *)"abc",  (uint8_t *)"12.34",
                            (uint8_t *)"-5",   (uint8_t *)"7.8" };
    uint8_t *buf;
    uint16_t pat_idx;

    total = 0;
    size--; /* Reserve space for null terminator */

    while (total < size) {
        seed++;
        pat_idx = (uint16_t)((seed ^ (seed >> 4)) & 0x7);
        buf     = patterns[pat_idx];
        next    = 0;

        /* Calculate pattern length */
        while (buf[next] != 0 && next < 10) {
            next++;
        }

        if (total + next + 1 < size) {
            for (i = 0; i < next; i++) {
                p[total + i] = buf[i];
            }
            p[total + next] = ',';
            total += next + 1;
        } else {
            break;
        }
    }

    /* Null terminate */
    while (total < size + 1) {
        p[total] = 0;
        total++;
    }
}

/* Function: du_bench_state
 * Benchmark state machine operations
 */
void du_bench_state(int16_t step)
{
    uint8_t *p       = mem_state;
    uint32_t blksize = sizeof(mem_state);
    uint32_t final_counts[NUM_STATES];
    uint32_t track_counts[NUM_STATES];
    uint32_t i;
    int16_t seed1 = 0x12;
    int16_t seed2 = 0x43;
    du_state_t fstate;

    /* Initialize counts */
    for (i = 0; i < NUM_STATES; i++) {
        final_counts[i] = 0;
        track_counts[i] = 0;
    }

    /* Run state machine over input */
    i = 0;
    while (*p != 0 && i < blksize) {
        fstate = du_state_transition(&p, track_counts);
        final_counts[fstate]++;
        i++;
        if (p >= (mem_state + blksize)) {
            break;
        }
    }

    /* Inject corruption */
    p = mem_state;
    i = 0;
    if (step <= 0) {
        step = 1; /* Avoid infinite loop */
    }
    while (i < blksize && p < (mem_state + blksize) && *p != 0) {
        if (*p != ',') {
            *p ^= (uint8_t)seed1;
        }
        i += (uint32_t)step;
        if (i >= blksize) {
            break;
        }
        p = mem_state + i;
    }

    /* Run state machine again */
    p = mem_state;
    i = 0;
    while (*p != 0 && i < blksize) {
        fstate = du_state_transition(&p, track_counts);
        final_counts[fstate]++;
        i++;
        if (p >= (mem_state + blksize)) {
            break;
        }
    }

    /* Restore input */
    p = mem_state;
    i = 0;
    if (step <= 0) {
        step = 1; /* Avoid infinite loop */
    }
    while (i < blksize && p < (mem_state + blksize) && *p != 0) {
        if (*p != ',') {
            *p ^= (uint8_t)seed2;
        }
        i += (uint32_t)step;
        if (i >= blksize) {
            break;
        }
        p = mem_state + i;
    }
}
