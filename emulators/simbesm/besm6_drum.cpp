/*
 * besm6_drum.c: BESM-6 magnetic drum device
 *
 * Copyright (c) 2009, Serge Vakulenko
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * SERGE VAKULENKO OR LEONID BROUKHIS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.

 * Except as contained in this notice, the name of Leonid Broukhis or
 * Serge Vakulenko shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from Leonid Broukhis and Serge Vakulenko.
 */
#include "besm6_defs.h"

/*
 * The control word of a magnetic drum transfer.
 */
#define DRUM_READ_OVERLAY 020000000 /* read with overlay */
#define DRUM_PARITY_FLAG                                                        \
    010000000                        /* suppress reading words with bad parity, \
                                      * or write with bad parity */
#define DRUM_READ_SYSDATA 004000000  /* read the system words only */
#define DRUM_PAGE_MODE    001000000  /* transfer a whole page */
#define DRUM_READ         000400000  /* read from the drum into memory */
#define DRUM_PAGE         000370000  /* memory page number */
#define DRUM_BLOCK        0740000000 /* memory block number - bits 27-24 */
#define DRUM_PARAGRAF     000006000  /* paragraph number */
#define DRUM_UNIT         000001600  /* drum number */
#define DRUM_CYLINDER     000000174  /* track number on the drum */
#define DRUM_SECTOR       000000003  /* sector number */

/*
 * Parameters of a transfer with an external device.
 */
int drum_op;     /* the transfer control word */
int drum_zone;   /* zone number on the drum */
int drum_sector; /* first sector number on the drum */
int drum_memory; /* first memory address */
int drum_nwords; /* number of words transferred */
int drum_fail;   /* per-channel error mask */

t_stat drum_event(UNIT *u);

/*
 * DRUM data structures
 *
 * drum_dev     DRUM device descriptor
 * drum_unit    DRUM unit descriptor
 * drum_reg     DRUM register list
 */
UNIT drum_unit[] = {
    { .action = drum_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = drum_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
};

t_stat drum_reset(DEVICE *dptr);
t_stat drum_attach(UNIT *uptr, const char *cptr);
t_stat drum_detach(UNIT *uptr);

DEVICE drum_dev = { .name     = "DRUM",
                    .units    = drum_unit,
                    .numunits = 2,
                    .reset    = &drum_reset,
                    .detach   = &drum_detach };

/*
 * Reset routine
 */
t_stat drum_reset(DEVICE *dptr)
{
    drum_op     = 0;
    drum_zone   = 0;
    drum_sector = 0;
    drum_memory = 0;
    drum_nwords = 0;
    sim_cancel(&drum_unit[0]);
    sim_cancel(&drum_unit[1]);
    return SCPE_OK;
}

t_stat drum_attach(UNIT *u, const char *cptr)
{
    t_stat s;

    s = attach_unit(u, cptr);
    if (s != SCPE_OK)
        return s;
    if (u == &drum_unit[0])
        GRP |= GRP_DRUM1_FREE;
    else
        GRP |= GRP_DRUM2_FREE;
    return SCPE_OK;
}

t_stat drum_detach(UNIT *u)
{
    if (u == &drum_unit[0])
        GRP &= ~GRP_DRUM1_FREE;
    else
        GRP &= ~GRP_DRUM2_FREE;
    return detach_unit(u);
}

/*
 * Write to the drum.
 */
t_stat drum_write(UNIT *u)
{
    int ctlr;
    t_value *sysdata;

    ctlr    = (u == &drum_unit[1]);
    sysdata = ctlr ? &memory[020] : &memory[010];
    img_write(u->image, ZONE_SIZE * drum_zone, sysdata, 8);
    img_write(u->image, ZONE_SIZE * drum_zone + 8, &memory[drum_memory], 1024);
    if (img_error(u->image))
        return SCPE_IOERR;
    return SCPE_OK;
}

t_stat drum_write_sector(UNIT *u)
{
    int ctlr;
    t_value *sysdata;

    ctlr    = (u == &drum_unit[1]);
    sysdata = ctlr ? &memory[020] : &memory[010];
    img_write(u->image, ZONE_SIZE * drum_zone + drum_sector * 2, &sysdata[drum_sector * 2], 2);
    img_write(u->image, ZONE_SIZE * drum_zone + 8 + drum_sector * 256, &memory[drum_memory], 256);
    if (img_error(u->image))
        return SCPE_IOERR;
    return SCPE_OK;
}

/*
 * Read from the drum.
 */
t_stat drum_read(UNIT *u)
{
    int ctlr;
    t_value *sysdata;

    ctlr    = (u == &drum_unit[1]);
    sysdata = ctlr ? &memory[020] : &memory[010];
    if (img_read(u->image, ZONE_SIZE * drum_zone, sysdata, 8) != 8) {
        /* Read from an unformatted drum */
        drum_fail |= 0100 >> ctlr;
        return SCPE_OK;
    }
    if (!(drum_op & DRUM_READ_SYSDATA) &&
        img_read(u->image, ZONE_SIZE * drum_zone + 8, &memory[drum_memory], 1024) != 1024) {
        /* Read from an unformatted drum */
        drum_fail |= 0100 >> ctlr;
        return SCPE_OK;
    }
    if (img_error(u->image))
        return SCPE_IOERR;
    return SCPE_OK;
}

t_stat drum_read_sector(UNIT *u)
{
    int ctlr;
    t_value *sysdata;

    ctlr    = (u == &drum_unit[1]);
    sysdata = ctlr ? &memory[020] : &memory[010];
    if (img_read(u->image, ZONE_SIZE * drum_zone + drum_sector * 2, &sysdata[drum_sector * 2], 2) !=
        2) {
        /* Read from an unformatted drum */
        drum_fail |= 0100 >> ctlr;
        return SCPE_OK;
    }
    if (!(drum_op & DRUM_READ_SYSDATA)) {
        if (img_read(u->image, ZONE_SIZE * drum_zone + 8 + drum_sector * 256, &memory[drum_memory],
                     256) != 256) {
            /* Read from an unformatted drum */
            drum_fail |= 0100 >> ctlr;
            return SCPE_OK;
        }
    }
    if (img_error(u->image))
        return SCPE_IOERR;
    return SCPE_OK;
}

static void clear_memory(t_value *p, int nwords)
{
    while (nwords-- > 0)
        *p++ = SET_PARITY(0, PARITY_NUMBER);
}

/*
 * Perform a drum access.
 */
t_stat drum(int ctlr, uint32 cmd)
{
    UNIT *u = &drum_unit[ctlr];

    drum_op = cmd;
    if (drum_op & DRUM_PAGE_MODE) {
        /* Page transfer */
        drum_nwords = 1024;
        drum_zone   = (cmd & (DRUM_UNIT | DRUM_CYLINDER)) >> 2;
        drum_sector = 0;
        drum_memory = (cmd & DRUM_PAGE) >> 2 | (cmd & DRUM_BLOCK) >> 8;
        if (drum_dev.dctrl)
            besm6_debug("### %s drum %c%d zone %02o mem %05o-%05o",
                        (drum_op & DRUM_READ) ? "read" : "write", ctlr + '1', (drum_zone >> 5 & 7),
                        drum_zone & 037, drum_memory, drum_memory + drum_nwords - 1);
        if (drum_op & DRUM_READ) {
            clear_memory(ctlr ? &memory[020] : &memory[010], 8);
            if (!(drum_op & DRUM_READ_SYSDATA))
                clear_memory(&memory[drum_memory], 1024);
        }
    } else {
        /* Sector transfer */
        drum_nwords = 256;
        drum_zone   = (cmd & (DRUM_UNIT | DRUM_CYLINDER)) >> 2;
        drum_sector = cmd & DRUM_SECTOR;
        drum_memory = (cmd & (DRUM_PAGE | DRUM_PARAGRAF)) >> 2 | (cmd & DRUM_BLOCK) >> 8;
        if (drum_dev.dctrl)
            besm6_debug("### %s drum %c%d zone %02o sector %d mem %05o-%05o",
                        (drum_op & DRUM_READ) ? "read" : "write", ctlr + '1', (drum_zone >> 5 & 7),
                        drum_zone & 037, drum_sector & 3, drum_memory,
                        drum_memory + drum_nwords - 1);
        if (drum_op & DRUM_READ) {
            clear_memory(ctlr ? &memory[020 + drum_sector * 2] : &memory[010 + drum_sector * 2], 2);
            if (!(drum_op & DRUM_READ_SYSDATA))
                clear_memory(&memory[drum_memory], 256);
        }
    }
    if (!u->image) {
        /* Device not attached. */
        drum_fail |= 0100 >> ctlr;
        return SCPE_OK;
    }
    drum_fail &= ~(0100 >> ctlr);
    if (drum_op & DRUM_READ_OVERLAY) {
        /* Not implemented. */
        return SCPE_NOFNC;
    }
    t_stat (*serve)(UNIT *u);
    if (drum_op & DRUM_READ) {
        serve = (drum_op & DRUM_PAGE_MODE) ? drum_read : drum_read_sector;
    } else {
        if (drum_op & DRUM_PARITY_FLAG) {
            besm6_log("### drum write with bad parity not implemented");
            return SCPE_NOFNC;
        }
        if (u->flags & UNIT_RO) {
            /* Read only. */
            return SCPE_RO;
        }
        serve = (drum_op & DRUM_PAGE_MODE) ? drum_write : drum_write_sector;
    }

    /* Clear the main interrupt register. */
    if (u == &drum_unit[0])
        GRP &= ~GRP_DRUM1_FREE;
    else
        GRP &= ~GRP_DRUM2_FREE;

    /* The driver performs the transfer and then waits for an event from the
     * device.  Per the figures in G. L. Mazny's book, 20 ms for the transfer,
     * or 200 thousand ticks. */
    /*io_post(u, serve, 20*MSEC);*/
    io_post(u, serve, 20 * USEC); /* sped up for debugging */
    return SCPE_OK;
}

/*
 * Event: a drum transfer has finished.
 * Set the interrupt flag.
 */
t_stat drum_event(UNIT *u)
{
    if (u == &drum_unit[0])
        GRP |= GRP_DRUM1_FREE;
    else
        GRP |= GRP_DRUM2_FREE;
    return SCPE_OK;
}

/*
 * Poll the transfer error register with instruction 033 4035.
 */
int drum_errors()
{
    return drum_fail;
}
