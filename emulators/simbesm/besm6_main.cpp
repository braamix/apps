/*
 * besm6_main.c: entry point that boots Unix with no command script.
 *
 * Copyright (c) 2009, Serge Vakulenko
 * Copyright (c) 2009, Leonid Broukhis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * SERGE VAKULENKO OR LEONID BROUKHIS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Leonid Broukhis or
 * Serge Vakulenko shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from Leonid Broukhis and Serge Vakulenko.
 */
#include "besm6_defs.h"

/*
 * What demo/unix.ini used to say, as C.  The order is its order: tty_attach()
 * reads the flags `set ttyNN raw8' sets.  See unix.ini for why each step is
 * needed.  Run from the directory holding the images.
 */
status_t besm6_boot_unix(Blob *kernel)
{
    status_t r;

    /* set cpu latin */
    besm6_latin = 1;

    /* set mmu cache */
    mmu_unit.flags |= CACHE_ENB;

    /* set tty25 raw8; set tty26 raw8 */
    tty_unit[25].flags = (tty_unit[25].flags & ~TTY_CHARSET_MASK) | TTY_RAW8_CHARSET;
    tty_unit[26].flags = (tty_unit[26].flags & ~TTY_CHARSET_MASK) | TTY_RAW8_CHARSET;

    sim_switches = 0;

    /* attach tty25 console */
    if ((r = tty_attach(&tty_unit[25], "console")) != SCPE_OK)
        return r;

    /* attach tty26: the second Consul line goes to a second screen where the
     * page put one up, and is turned off where it did not. */
    if ((r = tty_attach(&tty_unit[26], con_second() ? "screen2" : "none")) != SCPE_OK)
        return r;

    /* attach md00 root3072.disk; attach md01 usr3100.disk */
    if ((r = disk_attach(&md_unit[0], "root3072.disk")) != SCPE_OK)
        return r;
    if ((r = disk_attach(&md_unit[1], "usr3100.disk")) != SCPE_OK)
        return r;

    /* attach -n drum0/drum1.  sim_switches is the only channel for `-n', and
     * disk_attach above ORs in `-e'. */
    sim_switches = SWMASK('N');
    if ((r = drum_attach(&drum_unit[0], "unix0.drum")) != SCPE_OK)
        return r;
    if ((r = drum_attach(&drum_unit[1], "unix1.drum")) != SCPE_OK)
        return r;
    sim_switches = 0;

    /* load unix: besm6_load() sets PC from a_entry.  The platform read the
     * image: on Braam that is a co_await, and this is not one. */
    return sim_load(kernel);
}
