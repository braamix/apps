// deflate.cpp — deflate.c, by Jean-loup Gailly. LZ77 with lazy matching.
//
// Upstream, but for the four routines that reach a stream. read_buf is the
// only input and flush_block the only output, so lm_init, fill_window,
// deflate and deflate_fast are coroutines and everything else — longest_match
// included — is the plain code it was. longest_match is noinline: inlined
// into a coroutine, its locals would move into the heap-allocated frame.

#include "zip.h"

// Configuration parameters

// Compile with MEDIUM_MEM to reduce the memory requirements or
// with SMALL_MEM to use as little memory as possible. Use BIG_MEM if the
// entire input file can be held in memory (not possible on 16 bit systems).
// Warning: defining these symbols affects HASH_BITS (see below) and thus
// affects the compression ratio. The compressed output
// is still correct, and might even be smaller in some cases.

#ifndef HASH_BITS
#define HASH_BITS 15
// For portability to 16 bit machines, do not use values above 15.
#endif

#define HASH_SIZE (unsigned)(1 << HASH_BITS)
#define HASH_MASK (HASH_SIZE - 1)
#define WMASK     (WSIZE - 1)
// HASH_SIZE and WSIZE must be powers of two

#define NIL 0
// Tail of hash chains

#define FAST 4
#define SLOW 2
// speed options for the general purpose bit flag

#define TOO_FAR 4096
// Matches of length 3 are discarded if their distance exceeds TOO_FAR

// Local data used by the "longest match" routines.

typedef ush Pos;
typedef unsigned IPos;
// A Pos is an index in the character window. We use short instead of int to
// save space in the various tables. IPos is used only for parameter passing.

uch far *near window = NULL;
Pos far *near prev   = NULL;
Pos far *near head;
ulg window_size;
// window size, 2*WSIZE except for MMAP or BIG_MEM, where it is the
// input file length plus MIN_LOOKAHEAD.

long block_start;
// window position at the beginning of the current output block. Gets
// negative when the window is moved backwards.

local int sliding;
// Set to false when the input file is already in memory

local unsigned ins_h; // hash index of string to be inserted

#define H_SHIFT ((HASH_BITS + MIN_MATCH - 1) / MIN_MATCH)
// Number of bits by which ins_h and del_h must be shifted at each
// input step. It must be such that after MIN_MATCH steps, the oldest
// byte no longer takes part in the hash key, that is:
// H_SHIFT * MIN_MATCH >= HASH_BITS

unsigned int near prev_length;
// Length of the best match at previous step. Matches not greater than this
// are discarded. This is used in the lazy match evaluation.

unsigned near strstart;    // start of string to insert
unsigned near match_start; // start of matching string
local int eofile;          // flag set at end of input file
local unsigned lookahead;  // number of valid bytes ahead in window

unsigned near max_chain_length;
// To speed up deflation, hash chains are never searched beyond this length.
// A higher limit improves compression ratio but degrades the speed.

local unsigned int max_lazy_match;
// Attempt to find a better match only when the current match is strictly
// smaller than this value. This mechanism is used only for compression
// levels >= 4.
#define max_insert_length max_lazy_match
// Insert new strings in the hash table only if the match length
// is not greater than this length. This saves time but degrades compression.
// max_insert_length is used only for compression levels <= 3.

unsigned near good_match;
// Use a faster search when the previous match is longer than this

int near nice_match; // Stop searching when current match exceeds this

// Values for max_lazy_match, good_match, nice_match and max_chain_length,
// depending on the desired pack level (0..9). The values given below have
// been tuned to exclude worst case performance for pathological files.
// Better values may be found for specific files.

typedef struct config {
    ush good_length; // reduce lazy search above this match length
    ush max_lazy;    // do not perform lazy search above this match length
    ush nice_length; // quit search above this match length
    ush max_chain;
} config;

local config configuration_table[10] = {
    // good lazy nice chain
    /* 0 */ { 0, 0, 0, 0 }, // store only
    /* 1 */ { 4, 4, 8, 4 }, // maximum speed, no lazy matches
    /* 2 */ { 4, 5, 16, 8 },
    /* 3 */ { 4, 6, 32, 32 },

    /* 4 */ { 4, 4, 16, 16 }, // lazy matches
    /* 5 */ { 8, 16, 32, 32 },
    /* 6 */ { 8, 16, 128, 128 },
    /* 7 */ { 8, 32, 128, 256 },
    /* 8 */ { 32, 128, 258, 1024 },
    /* 9 */ { 32, 258, 258, 4096 }
}; // maximum compression

// Note: the deflate() code requires max_lazy >= MIN_MATCH and max_chain >= 4
// For deflate_fast() (levels <= 3) good is ignored and lazy has a different
// meaning.

#define EQUAL 0
// result of memcmp for equal strings

// Prototypes for local functions.

local Task<void> fill_window OF((void));

local Task<uzoff_t> deflate_fast OF((void)); // now use uzoff_t 7/24/04 EG

__attribute__((noinline)) int longest_match OF((IPos cur_match));

// Update a hash value with the given input byte
// IN  assertion: all calls to to UPDATE_HASH are made with consecutive
// input characters, so that a running hash key can be computed from the
// previous key instead of complete recalculation each time.
#define UPDATE_HASH(h, c) (h = (((h) << H_SHIFT) ^ (c)) & HASH_MASK)

// Insert string s in the dictionary and set match_head to the previous head
// of the hash chain (the most recent string with same hash key). Return
// the previous length of the hash chain.
// IN  assertion: all calls to to INSERT_STRING are made with consecutive
// input characters and the first MIN_MATCH bytes of s are valid
// (except for the last MIN_MATCH-1 bytes of the input file).
#define INSERT_STRING(s, match_head)                    \
    (UPDATE_HASH(ins_h, window[(s) + (MIN_MATCH - 1)]), \
     prev[(s) & WMASK] = match_head = head[ins_h], head[ins_h] = (s))

// Initialize the "longest match" routines for a new file
//
// IN assertion: window_size is > 0 if the input file is already read or
// mmap'ed in the window[] array, 0 otherwise. In the first case,
// window_size is sufficient to contain the whole input file plus
// MIN_LOOKAHEAD bytes (to avoid referencing memory beyond the end
// of window[] when looking for matches towards the end).
Task<void> lm_init(int pack_level, ush *flags)
{
    unsigned j;

    if (pack_level < 1 || pack_level > 9) {
        error("bad pack level");
        co_return;
    }

    // Do not slide the window if the whole input is already in memory
    // (window_size > 0)
    sliding = 0;
    if (window_size == 0L) {
        sliding     = 1;
        window_size = (ulg)2L * WSIZE;
    }

    // Use dynamic allocation if compiler does not like big static arrays:
    if (window == NULL) {
        window = (uch far *)zcalloc(WSIZE, 2 * sizeof(uch));
        if (window == NULL) {
            zip_fail(ZE_MEM, "window allocation");
            co_return;
        }
    }
    if (prev == NULL) {
        prev = (Pos far *)zcalloc(WSIZE, sizeof(Pos));
        head = (Pos far *)zcalloc(HASH_SIZE, sizeof(Pos));
        if (prev == NULL || head == NULL) {
            zip_fail(ZE_MEM, "hash table allocation");
            co_return;
        }
    }

    // Initialize the hash table (avoiding 64K overflow for 16 bit systems).
    // prev[] will be initialized on the fly.
    head[HASH_SIZE - 1] = NIL;
    memset((char *)head, NIL, (unsigned)(HASH_SIZE - 1) * sizeof(*head));

    // Set the default configuration parameters:
    max_lazy_match   = configuration_table[pack_level].max_lazy;
    good_match       = configuration_table[pack_level].good_length;
    nice_match       = configuration_table[pack_level].nice_length;
    max_chain_length = configuration_table[pack_level].max_chain;
    if (pack_level <= 2) {
        *flags |= FAST;
    } else if (pack_level >= 8) {
        *flags |= SLOW;
    }
    // ??? reduce max_chain_length for binary files

    strstart    = 0;
    block_start = 0L;

    j = WSIZE;
    if (sizeof(int) > 2)
        j <<= 1; // Can read 64K in one step
    lookahead = co_await (*read_buf)((char *)window, j);

    if (lookahead == 0 || lookahead == (unsigned)EOF) {
        eofile = 1, lookahead = 0;
        co_return;
    }
    eofile = 0;
    // Make sure that we always have enough lookahead. This is important
    // if input comes from a device such as a tty.
    if (lookahead < MIN_LOOKAHEAD)
        co_await fill_window();

    ins_h = 0;
    for (j = 0; j < MIN_MATCH - 1; j++)
        UPDATE_HASH(ins_h, window[j]);
    // If lookahead < MIN_MATCH, ins_h is garbage, but this is
    // not important since only literal bytes will be emitted.
}

// Free the window and hash table
void lm_free()
{
    if (window != NULL) {
        zcfree(window);
        window = NULL;
    }
    if (prev != NULL) {
        zcfree(prev);
        zcfree(head);
        prev = head = NULL;
    }
}

// Set match_start to the longest match starting at the given string and
// return its length. Matches shorter or equal to prev_length are discarded,
// in which case the result is equal to prev_length and match_start is
// garbage.
// IN assertions: cur_match is the head of the hash chain for the current
// string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
// For 80x86 and 680x0 and ARM, an optimized version is in match.asm or
// match.S. The code is functionally equivalent, so you can use the C version
// if desired.
__attribute__((noinline)) int longest_match(IPos cur_match)
{
    unsigned chain_length = max_chain_length;  // max hash chain length
    uch far *scan         = window + strstart; // current string
    uch far *match;                            // matched string
    int len;                                   // length of current match
    int best_len = prev_length;                // best match length so far
    IPos limit   = strstart > (IPos)MAX_DIST ? strstart - (IPos)MAX_DIST : NIL;
    // Stop when cur_match becomes <= limit. To simplify the code,
    // we prevent matches with the string of window index 0.

// The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
// It is easy to get rid of this optimization if necessary.
#if HASH_BITS < 8 || MAX_MATCH != 258
error:
    Code too clever
#endif

        uch far *strend = window + strstart + MAX_MATCH;
    uch scan_end1       = scan[best_len - 1];
    uch scan_end        = scan[best_len];

    // Do not waste too much time if we already have a good match:
    if (prev_length >= good_match) {
        chain_length >>= 2;
    }

    Assert(strstart <= window_size - MIN_LOOKAHEAD, "insufficient lookahead");

    do {
        Assert(cur_match < strstart, "no future");
        match = window + cur_match;

        // Skip to next match if the match length cannot increase
        // or if the match length is less than 2:

        if (match[best_len] != scan_end || match[best_len - 1] != scan_end1 || *match != *scan ||
            *++match != scan[1])
            continue;

        // The check at best_len-1 can be removed because it will be made
        // again later. (This heuristic is not always a win.)
        // It is not necessary to compare scan[2] and match[2] since they
        // are always equal when the other bytes match, given that
        // the hash keys are equal and that HASH_BITS >= 8.
        scan += 2, match++;

        // We check for insufficient lookahead only every 8th comparison;
        // the 256th check will be made at strstart+258.
        do {
        } while (*++scan == *++match && *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match && scan < strend);

        Assert(scan <= window + (unsigned)(window_size - 1), "wild scan");

        len  = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;

        if (len > best_len) {
            match_start = cur_match;
            best_len    = len;
            if (len >= nice_match)
                break;
            scan_end1 = scan[best_len - 1];
            scan_end  = scan[best_len];
        }
    } while ((cur_match = prev[cur_match & WMASK]) > limit && --chain_length != 0);

    return best_len;
}

#define check_match(start, match, length)

// Flush the current block, with given end-of-file flag.
// IN assertion: strstart is set to the end of the current match.
local Task<uzoff_t> flush_block_now OF((int eof));

#define FLUSH_BLOCK(eof) (co_await flush_block_now(eof))

// flush_block, and then the bytes it left in the sink. trees.cpp cannot write
// them itself: it is not a coroutine and must not become one. The sink holds
// one block at most, because this drains it before the next.
local Task<uzoff_t> flush_block_now(int eof)
{
    uzoff_t n =
        flush_block(block_start >= 0L ? (char *)&window[(unsigned)block_start] : (char *)NULL,
                    (ulg)strstart - (ulg)block_start, eof);
    co_await defl_drain();
    co_return n;
}

// Fill the window when the lookahead becomes insufficient.
// Updates strstart and lookahead, and sets eofile if end of input file.
//
// IN assertion: lookahead < MIN_LOOKAHEAD && strstart + lookahead > 0
// OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
// At least one byte has been read, or eofile is set; file reads are
// performed for at least two bytes (required for the translate_eol option).
local Task<void> fill_window()
{
    unsigned n, m;
    unsigned more; // Amount of free space at the end of the window.

    do {
        more = (unsigned)(window_size - (ulg)lookahead - (ulg)strstart);

        // If the window is almost full and there is insufficient lookahead,
        // move the upper half to the lower one to make room in the upper half.
        if (more == (unsigned)EOF) {
            // Very unlikely, but possible on 16 bit machine if strstart == 0
            // and lookahead == 1 (input done one byte at time)
            more--;

            // For MMAP or BIG_MEM, the whole input file is already in memory so
            // we must not perform sliding. We must however call (*read_buf)() in
            // order to compute the crc, update lookahead and possibly set eofile.
        } else if (strstart >= WSIZE + MAX_DIST && sliding) {
            // By the IN assertion, the window is not empty so we can't confuse
            // more == 0 with more == 64K on a 16 bit machine.
            memcpy((char *)window, (char *)window + WSIZE, (unsigned)WSIZE);
            match_start -= WSIZE;
            strstart -= WSIZE; // we now have strstart >= MAX_DIST:

            block_start -= (long)WSIZE;

            for (n = 0; n < HASH_SIZE; n++) {
                m       = head[n];
                head[n] = (Pos)(m >= WSIZE ? m - WSIZE : NIL);
            }
            for (n = 0; n < WSIZE; n++) {
                m       = prev[n];
                prev[n] = (Pos)(m >= WSIZE ? m - WSIZE : NIL);
                // If n is not on any hash chain, prev[n] is garbage but
                // its value will never be used.
            }
            more += WSIZE;
            if (dot_size > 0 && !display_globaldots) {
                // initial space
                if (noisy && dot_count == -1) {
                    co_await b_fputc(' ', mesg);
                    co_await b_fflush(mesg);
                    dot_count++;
                }
                dot_count++;
                if (dot_size <= (dot_count + 1) * WSIZE)
                    dot_count = 0;
            }
            if ((verbose || noisy) && dot_size && !dot_count) {
                co_await b_fputc('.', mesg);
                co_await b_fflush(mesg);
                mesg_line_started = 1;
            }
        }
        if (eofile)
            co_return;

        // If there was no sliding:
        // strstart <= WSIZE+MAX_DIST-1 && lookahead <= MIN_LOOKAHEAD - 1 &&
        // more == window_size - lookahead - strstart
        // => more >= window_size - (MIN_LOOKAHEAD-1 + WSIZE + MAX_DIST-1)
        // => more >= window_size - 2*WSIZE + 2
        // In the MMAP or BIG_MEM case (not yet supported in gzip),
        // window_size == input_size + MIN_LOOKAHEAD  &&
        // strstart + lookahead <= input_size => more >= MIN_LOOKAHEAD.
        // Otherwise, window_size == 2*WSIZE so more >= 2.
        // If there was sliding, more >= WSIZE. So in all cases, more >= 2.
        Assert(more >= 2, "more < 2");

        n = co_await (*read_buf)((char *)window + strstart + lookahead, more);
        if (n == 0 || n == (unsigned)EOF) {
            eofile = 1;
        } else {
            lookahead += n;
        }
    } while (lookahead < MIN_LOOKAHEAD && !eofile);
}

// Processes a new input file and return its compressed length. This
// function does not perform lazy evaluation of matches and inserts
// new strings in the dictionary only for unmatched strings or for short
// matches. It is used only for the fast compression options.
local Task<uzoff_t> deflate_fast()
{
    IPos hash_head = NIL;      // head of the hash chain
    int flush;                 // set if current block must be flushed
    unsigned match_length = 0; // length of best match

    prev_length = MIN_MATCH - 1;
    while (lookahead != 0) {
        // Insert the string window[strstart .. strstart+2] in the
        // dictionary, and set hash_head to the head of the hash chain:
        if (lookahead >= MIN_MATCH)
            INSERT_STRING(strstart, hash_head);

        // Find the longest match, discarding those <= prev_length.
        // At this point we have always match_length < MIN_MATCH
        if (hash_head != NIL && strstart - hash_head <= MAX_DIST) {
            // To simplify the code, we prevent matches with the string
            // of window index 0 (in particular we have to avoid a match
            // of the string with itself at the start of the input file).
            // Do not look for matches beyond the end of the input.
            // This is necessary to make deflate deterministic.
            if ((unsigned)nice_match > lookahead)
                nice_match = (int)lookahead;
            match_length = longest_match(hash_head);
            // longest_match() sets match_start
            if (match_length > lookahead)
                match_length = lookahead;
        }
        if (match_length >= MIN_MATCH) {
            check_match(strstart, match_start, match_length);

            flush = ct_tally(strstart - match_start, match_length - MIN_MATCH);

            lookahead -= match_length;

            // Insert new strings in the hash table only if the match length
            // is not too large. This saves time but degrades compression.
            if (match_length <= max_insert_length && lookahead >= MIN_MATCH) {
                match_length--; // string at strstart already in hash table
                do {
                    strstart++;
                    INSERT_STRING(strstart, hash_head);
                    // strstart never exceeds WSIZE-MAX_MATCH, so there are
                    // always MIN_MATCH bytes ahead.
                } while (--match_length != 0);
                strstart++;
            } else {
                strstart += match_length;
                match_length = 0;
                ins_h        = window[strstart];
                UPDATE_HASH(ins_h, window[strstart + 1]);
#if MIN_MATCH != 3
                Call UPDATE_HASH() MIN_MATCH - 3 more times
#endif
            }
        } else {
            // No match, output a literal byte
            Tracevv((stderr, "%c", window[strstart]));
            flush = ct_tally(0, window[strstart]);
            lookahead--;
            strstart++;
        }
        if (flush)
            FLUSH_BLOCK(0), block_start = strstart;

        // Make sure that we always have enough lookahead, except
        // at the end of the input file. We need MAX_MATCH bytes
        // for the next match, plus MIN_MATCH bytes to insert the
        // string following the next match.
        if (lookahead < MIN_LOOKAHEAD)
            co_await fill_window();
    }
    co_return FLUSH_BLOCK(1); // eof
}

// Same as above, but achieves better compression. We use a lazy
// evaluation for matches: a match is finally adopted only if there is
// no better match at the next window position.
Task<uzoff_t> deflate()
{
    IPos hash_head = NIL;                  // head of hash chain
    IPos prev_match;                       // previous match
    int flush;                             // set if current block must be flushed
    int match_available   = 0;             // set if previous match exists
    unsigned match_length = MIN_MATCH - 1; // length of best match

    if (level <= 3)
        co_return co_await deflate_fast(); // optimized for speed

    // Process the input block.
    while (lookahead != 0) {
        // Insert the string window[strstart .. strstart+2] in the
        // dictionary, and set hash_head to the head of the hash chain:
        if (lookahead >= MIN_MATCH)
            INSERT_STRING(strstart, hash_head);

        // Find the longest match, discarding those <= prev_length.
        prev_length = match_length, prev_match = match_start;
        match_length = MIN_MATCH - 1;

        if (hash_head != NIL && prev_length < max_lazy_match && strstart - hash_head <= MAX_DIST) {
            // To simplify the code, we prevent matches with the string
            // of window index 0 (in particular we have to avoid a match
            // of the string with itself at the start of the input file).
            // Do not look for matches beyond the end of the input.
            // This is necessary to make deflate deterministic.
            if ((unsigned)nice_match > lookahead)
                nice_match = (int)lookahead;
            match_length = longest_match(hash_head);
            // longest_match() sets match_start
            if (match_length > lookahead)
                match_length = lookahead;

            // Ignore a length 3 match if it is too distant:
            if (match_length == MIN_MATCH && strstart - match_start > TOO_FAR) {
                // If prev_match is also MIN_MATCH, match_start is garbage
                // but we will ignore the current match anyway.
                match_length = MIN_MATCH - 1;
            }
        }
        // If there was a match at the previous step and the current
        // match is not better, output the previous match:
        if (prev_length >= MIN_MATCH && match_length <= prev_length) {
            unsigned max_insert = strstart + lookahead - MIN_MATCH;

            check_match(strstart - 1, prev_match, prev_length);

            flush = ct_tally(strstart - 1 - prev_match, prev_length - MIN_MATCH);

            // Insert in hash table all strings up to the end of the match.
            // strstart-1 and strstart are already inserted.
            lookahead -= prev_length - 1;
            prev_length -= 2;
            do {
                if (++strstart <= max_insert) {
                    INSERT_STRING(strstart, hash_head);
                    // strstart never exceeds WSIZE-MAX_MATCH, so there are
                    // always MIN_MATCH bytes ahead.
                }
            } while (--prev_length != 0);
            strstart++;
            match_available = 0;
            match_length    = MIN_MATCH - 1;

            if (flush)
                FLUSH_BLOCK(0), block_start = strstart;

        } else if (match_available) {
            // If there was no match at the previous position, output a
            // single literal. If there was a match but the current match
            // is longer, truncate the previous match to a single literal.
            Tracevv((stderr, "%c", window[strstart - 1]));
            if (ct_tally(0, window[strstart - 1])) {
                FLUSH_BLOCK(0), block_start = strstart;
            }
            strstart++;
            lookahead--;
        } else {
            // There is no previous match to compare with, wait for
            // the next step to decide.
            match_available = 1;
            strstart++;
            lookahead--;
        }
        Assert(strstart <= isize && lookahead <= isize, "a bit too far");

        // Make sure that we always have enough lookahead, except
        // at the end of the input file. We need MAX_MATCH bytes
        // for the next match, plus MIN_MATCH bytes to insert the
        // string following the next match.
        if (lookahead < MIN_LOOKAHEAD)
            co_await fill_window();
    }
    if (match_available)
        ct_tally(0, window[strstart - 1]);

    co_return FLUSH_BLOCK(1); // eof
}
