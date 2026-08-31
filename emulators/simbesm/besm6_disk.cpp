/*
 * BESM-6 magnetic disk device
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
 * The control word of a magnetic disk transfer.
 */
#define DISK_BLOCK        0740000000 /* memory block number - bits 27-24 */
#define DISK_READ_SYSDATA 004000000  /* read the system words only */
#define DISK_PAGE_MODE    001000000  /* transfer a whole page */
#define DISK_READ         000400000  /* read from the disk into memory */
#define DISK_PAGE         000370000  /* memory page number */
#define DISK_HALFPAGE     000004000  /* which half of the page */
#define DISK_UNIT         000001600  /* unit number */
#define DISK_HALFZONE     000000001  /* which half of the zone */

/*
 * Status register bits (most are unused: error conditions are not simulated)
 */
#define STATUS_SEEK       000000377 /* "Seek done" mask, per unit */
#define STATUS_READY      000000400 /* Selected unit is ready */
#define STATUS_SEEK_FAIL  000001000 /* Head location unknown, unit not ready */
#define STATUS_CHECKSUM   000002000 /* Bad checksum on read */
#define STATUS_FAILURE    000004000 /* Failure, OR of some upper bits */
#define STATUS_MAYDAY     000010000 /* Unspecified failure */
#define STATUS_NO_AMRK    000020000 /* Address marker not found after a revolution */
#define STATUS_WRONG_CYL  000040000 /* Wrong address marker */
#define STATUS_WRONG_ID   000100000 /* Bad track ID */
#define STATUS_BAD_ACSUM  000200000 /* Bad checksum of the address marker */
#define STATUS_UNFINISHED 000400000 /* IO not finished after a revolution */
#define STATUS_TRK_PARITY 001000000 /* Track parity in two-track IO */
#define STATUS_READONLY   002000000 /* The selected unit is read-only */
#define STATUS_POWERUP    004000000 /* The unit is powered up */
#define STATUS_ABSENT     010000000 /* The unit is not connected */
#define STATUS_BUF_ERR    020000000 /* Transfer buffer not ready */

/*
 * Total size of a "7.25 Mb" disk is 1000 (decimal) blocks;
 * of a "29 Mb" disk - 4000 blocks, out of which 4 are so called
 * pre-blocks. Logical blocks are mapped to physical by adding 4.
 * Physical blocks 0 to 2 are accesible only by standalone programs,
 * block 3 has the logical number "minus one".
 */
/*
 * Parameters of a transfer with an external device.
 */
typedef struct {
    int op;           /* the transfer control word */
    int group;        /* Unit group number */
    int dev;          /* unit number, 0..7 */
    int zone;         /* zone number on the disk */
    int track;        /* which half of the zone */
    int memory;       /* first memory address */
    int format;       /* the formatting flag */
    int status;       /* the status register */
    t_value mask_grp; /* the ГРП ready mask */
    int mask_fail;    /* the transfer error mask */
    t_value *sysdata; /* the system data buffer */
} KMD;

static KMD controller[2]; /* the two КМД cabinets */
int disk_fail;            /* per-channel error mask */

t_stat disk_event(UNIT *u);

/*
 * DISK data structures
 *
 * md_dev     DISK device descriptor
 * md_unit    DISK unit descriptor
 * md_reg     DISK register list
 */
UNIT md_unit[64] = {
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
    { .action = disk_event, .flags = UNIT_ATTABLE | UNIT_ROABLE },
};

#define DISK_TYPE_MASK  (1 << UNIT_V_UF)
#define DISK_TYPE_7_25M 0
#define DISK_TYPE_29M   (1 << UNIT_V_UF)
#define IS_29MB(u)      (((u)->flags & DISK_TYPE_MASK) == DISK_TYPE_29M)

t_stat disk_reset(DEVICE *dptr);
t_stat disk_attach(UNIT *uptr, const char *cptr);
t_stat disk_detach(UNIT *uptr);

#define DEB_OPS 000001
#define DEB_RRD 000002
#define DEB_RWR 000004
#define DEB_INT 000010
#define DEB_TRC 000020
#define DEB_DAT 000040
#define DEB_STA 000100

DEVICE md_dev[8] = { { .name     = "MD0",
                       .units    = md_unit,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD1",
                       .units    = md_unit + 8,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD2",
                       .units    = md_unit + 16,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD3",
                       .units    = md_unit + 24,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD4",
                       .units    = md_unit + 32,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD5",
                       .units    = md_unit + 40,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD6",
                       .units    = md_unit + 48,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach },
                     { .name     = "MD7",
                       .units    = md_unit + 56,
                       .numunits = 8,
                       .reset    = &disk_reset,
                       .detach   = &disk_detach } };

/*
 * Find the controller a unit belongs to.
 */
static KMD *unit_to_ctlr(UNIT *u)
{
    return &controller[(u - md_unit) / 32];
}

/*
 * Reset routine
 */
t_stat disk_reset(DEVICE *dptr)
{
    int i;
    int ctlr       = (dptr - md_dev) / 4;
    int first_unit = (dptr - md_dev) * 8;
    KMD *c         = &controller[ctlr];
    memset(c, 0, sizeof(KMD));
    c->sysdata   = &memory[030 + ctlr * 8];
    c->mask_grp  = GRP_CHAN3_FREE >> ctlr;
    c->mask_fail = 020 >> ctlr;
    for (i = first_unit; i < first_unit + 8; ++i) {
        md_unit[i].dptr = dptr;
        sim_cancel(&md_unit[i]);
    }
    return SCPE_OK;
}

t_stat disk_attach(UNIT *u, const char *cptr)
{
    t_stat s;
    int32 saved_switches = sim_switches;
    sim_switches |= SWMASK('E');

    while (1) {
        s = attach_unit(u, cptr);
        if ((s == SCPE_OK) && (sim_switches & SWMASK('N'))) {
            t_value control[4]; /* block (zone) number, key, userid, checksum */
            int diskno, blkno, word;
            char *filenamepart = NULL;
            const char *pos;
            /* Using the rightmost sequence of digits within the filename
             * provided in the command line as a volume number,
             * e.g. "/var/tmp/besm6/2052.bin" -> 2052
             */
            filenamepart = sim_basename(u->filename);
            pos          = filenamepart + strlen(filenamepart);
            while (pos > filenamepart && !isdigit(*--pos))
                ;
            while (pos > filenamepart && isdigit(*pos))
                --pos;
            if (!isdigit(*pos))
                ++pos;
            diskno = strtoul(pos, NULL, 10);
            free(filenamepart);
            if (diskno < 2048 || diskno > 4095) {
                if (diskno == 0)
                    s = sim_messagef(SCPE_ARG,
                                     "%s: filename must contain volume number 2048..4095\n",
                                     sim_uname(u));
                else
                    s = sim_messagef(
                        SCPE_ARG,
                        "%s: disk volume %d from filename %s invalid (must be 2048..4095)\n",
                        sim_uname(u), diskno, cptr);
                filenamepart = strdup(u->filename);
                detach_unit(u);
                remove(filenamepart);
                free(filenamepart);
                return s; /* not formatting */
            }
            sim_messagef(SCPE_OK, "%s: formatting disk volume %d\n", sim_uname(u), diskno);

            control[1] = SET_PARITY(0, PARITY_NUMBER);
            control[2] = SET_PARITY(0, PARITY_NUMBER);
            control[3] = SET_PARITY(0, PARITY_NUMBER);

            control[1] |= 01370707LL << 24; /* Magic mark */
            control[1] |= diskno << 12;

            /* Unlike the O/S routine, does not format the (useless) reserve tracks */
            for (blkno = 0; blkno < (IS_29MB(u) ? 4000 : 1000); ++blkno) {
                uint32 val = IS_29MB(u) ? blkno : 2 * blkno;
                control[0] = SET_PARITY((t_value)val << 36, PARITY_NUMBER);
                fwrite(control, sizeof(t_value), 4, u->fileref);
                control[0] = SET_PARITY((t_value)(val + 1) << 36, PARITY_NUMBER);
                fwrite(control, sizeof(t_value), 4, u->fileref);
                for (word = 0; word < 02000; ++word) {
                    fwrite(control + 2, sizeof(t_value), 1, u->fileref);
                }
            }
        }
        if (s == SCPE_OK || (saved_switches & SWMASK('E')) || (sim_switches & SWMASK('N')))
            break;
        sim_switches |= SWMASK('N');
    }
    return SCPE_OK;
}

t_stat disk_detach(UNIT *u)
{
    /* TODO: clear the channel's ГРП ready bit when the last disk is detached. */
    return detach_unit(u);
}

t_value spread(t_value val)
{
    int i, j;
    t_value res = 0;

    for (i = 0; i < 5; i++)
        for (j = 0; j < 9; j++)
            if (val & (1LL << (i + j * 5)))
                res |= 1LL << (i * 9 + j);
    return res & BITS48;
}

/*
 * Debug dump of a transferred data array.
 */
static void log_data(t_value *data, int nwords)
{
    int i;
    t_value val;

    if (!sim_deb)
        return;
    for (i = 0; i < nwords; ++i) {
        val = data[i];
        fprintf(sim_deb, " %04o-%04o-%04o-%04o", (int)(val >> 36) & 07777, (int)(val >> 24) & 07777,
                (int)(val >> 12) & 07777, (int)val & 07777);
        if ((i & 3) == 3)
            fprintf(sim_deb, "\n");
    }
    if ((i & 3) != 0)
        fprintf(sim_deb, "\n");
}

/*
 * Addition with carry to the right.
 */
static unsigned sum_with_right_carry(unsigned a, unsigned b)
{
    unsigned c;

    while (b) {
        c = a & b;
        a ^= b;
        b = c >> 1;
    }
    return a;
}

/*
 * Write to the disk.
 */
t_stat disk_write(UNIT *u)
{
    KMD *c = unit_to_ctlr(u);
    if (u->dptr->dctrl & DEB_DAT)
        besm6_debug("::: disk %02o write zone %04o mem %05o-%05o", c->dev, c->zone, c->memory,
                    c->memory + 1023);
    if (fseek(u->fileref, ZONE_SIZE * c->zone * 8, SEEK_SET) == 0) {
        fwrite(c->sysdata, 8, 8, u->fileref);
        fwrite(&memory[c->memory], 8, 1024, u->fileref);
    }

    if (ferror(u->fileref))
        return SCPE_IOERR;
    return SCPE_OK;
}

t_stat disk_write_track(UNIT *u)
{
    KMD *c = unit_to_ctlr(u);
    if (u->dptr->dctrl & DEB_DAT)
        besm6_debug("::: disk %02o write half-zone %04o.%d mem %05o-%05o", c->dev, c->zone,
                    c->track, c->memory, c->memory + 511);
    if (fseek(u->fileref, (ZONE_SIZE * c->zone + 4 * c->track) * 8, SEEK_SET) == 0) {
        fwrite(c->sysdata + 4 * c->track, 8, 4, u->fileref);
        if (fseek(u->fileref, (8 + ZONE_SIZE * c->zone + 512 * c->track) * 8, SEEK_SET) == 0) {
            fwrite(&memory[c->memory], 8, 512, u->fileref);
        }
    }
    if (ferror(u->fileref))
        return SCPE_IOERR;
    return SCPE_OK;
}

/*
 * Format a track.
 */
void disk_format(UNIT *u)
{
    KMD *c = unit_to_ctlr(u);
    t_value fmtbuf[5];
    t_value *ptr;
    int i;
    /* In effect the emulator has nothing to do. */
    if (!(u->dptr->dctrl & DEB_DAT))
        return;

    /* Find the start of the header being written. */
    ptr = &memory[c->memory];
    while ((*ptr & BITS48) == 0)
        ptr++;

    /* Decode from the comb form into the normal one. */
    for (i = 0; i < 5; i++)
        fmtbuf[i] = spread(ptr[i]);

    /* On the first formatting attempt the address marker starts in the top
     * 5-bit syllable, so skip the first syllable. */
    for (i = 0; i < 5; i++)
        fmtbuf[i] = ((fmtbuf[i] & BITS48) << 5) | (i == 4 ? 0 : (fmtbuf[i + 1] >> 40) & BITS(5));

    log_data(fmtbuf, 5);

    /* Print the identifier, the address and the address checksum. */
    if (u->dptr->dctrl & DEB_TRC) {
        if (IS_29MB(u))
            besm6_debug("::: disk %02o format zone %04o mem %05o skip %02o hdr %010o %010o", c->dev,
                        c->zone, c->memory, ptr - memory - c->memory,
                        (int)(fmtbuf[0] >> 8 & BITS(30)), (int)(fmtbuf[2] >> 14 & BITS(30)));
        else
            besm6_debug("::: disk %02o format half-zone %04o.%d mem %05o skip %02o hdr %010o %010o",
                        c->dev, c->zone, c->track, c->memory, (uint32)(ptr - memory - c->memory),
                        (int)(fmtbuf[0] >> 8 & BITS(30)), (int)(fmtbuf[2] >> 14 & BITS(30)));
    }
}

/*
 * Read from the disk.
 */
t_stat disk_read(UNIT *u)
{
    KMD *c = unit_to_ctlr(u);
    if (u->dptr->dctrl & DEB_DAT)
        besm6_debug((c->op & DISK_READ_SYSDATA) ? "::: disk %02o read zone %04o system words"
                                                : "::: disk %02o read zone %04o mem %05o-%05o",
                    c->dev, c->zone, c->memory, c->memory + 1023);
    if (fseek(u->fileref, ZONE_SIZE * c->zone * 8, SEEK_SET) != 0 ||
        fread(c->sysdata, 8, 8, u->fileref) != 8) {
        /* Read from an unformatted disk */
        disk_fail |= c->mask_fail;
        return SCPE_OK;
    }
    if (!(c->op & DISK_READ_SYSDATA) && fread(&memory[c->memory], 8, 1024, u->fileref) != 1024) {
        /* Read from an unformatted disk */
        disk_fail |= c->mask_fail;
        return SCPE_OK;
    }

    if (ferror(u->fileref))
        return SCPE_IOERR;
    return SCPE_OK;
}

t_value collect(t_value val)
{
    int i, j;
    t_value res = 0;

    for (i = 0; i < 5; i++)
        for (j = 0; j < 9; j++)
            if (val & (1LL << (i * 9 + j)))
                res |= 1LL << (i + j * 5);
    return res & BITS48;
}

t_stat disk_read_track(UNIT *u)
{
    KMD *c = unit_to_ctlr(u);
    if (u->dptr->dctrl & DEB_DAT)
        besm6_debug((c->op & DISK_READ_SYSDATA)
                        ? "::: disk %02o read half-zone %04o.%d system words"
                        : "::: disk %02o read half-zone %04o.%d mem %05o-%05o",
                    c->dev, c->zone, c->track, c->memory, c->memory + 511);
    if (fseek(u->fileref, (ZONE_SIZE * c->zone + 4 * c->track) * 8, SEEK_SET) != 0 ||
        fread(c->sysdata + 4 * c->track, 8, 4, u->fileref) != 4) {
        /* Read from an unformatted disk */
        disk_fail |= c->mask_fail;
        return SCPE_OK;
    }
    if (!(c->op & DISK_READ_SYSDATA)) {
        if (fseek(u->fileref, (8 + ZONE_SIZE * c->zone + 512 * c->track) * 8, SEEK_SET) != 0 ||
            fread(&memory[c->memory], 8, 512, u->fileref) != 512) {
            /* Read from an unformatted disk */
            disk_fail |= c->mask_fail;
            return SCPE_OK;
        }
    }
    if (ferror(u->fileref))
        return SCPE_IOERR;
    return SCPE_OK;
}

/*
 * Read a track header.
 */
void disk_read_header(UNIT *u)
{
    KMD *c           = unit_to_ctlr(u);
    t_value *sysdata = IS_29MB(u) ? c->sysdata : c->sysdata + 4 * c->track;
    int iaksa, i, cyl, head;
    int reserve_start = IS_29MB(u) ? 07640 : 01750;

    /* The address: cylinder and head number. */
    if (IS_29MB(u)) {
        head = c->zone;
        cyl  = head / 20;
        head %= 20;
        iaksa = (head << 3) + (cyl << 8);
        iaksa <<= 12;
    } else {
        head = (c->zone << 1) + c->track;
        cyl  = head / 10;
        head %= 10;
        iaksa = (cyl << 20) | (head << 16);
    }

    /* The spare track identifier. */
    if (c->zone >= reserve_start)
        iaksa |= BBIT(30);

    /* The address checksum, added with carry to the right. */
    iaksa |= BITS(12) & ~sum_with_right_carry(iaksa >> 12, iaksa >> 24);

    /* An address marker, 42 zeros, an address marker, many ones. */
    sysdata[0] = 07404000000000000LL | (t_value)iaksa << 8;
    sysdata[1] = 03740LL;
    sysdata[2] = 00400000000037777LL | (t_value)iaksa << 14;
    sysdata[3] = BITS48;

    if (IS_29MB(u)) {
        for (i = 0; i < 4; i++) {
            memory[c->memory + i + 014] =
                SET_PARITY(sysdata[i] & 0777777777777777LL, PARITY_NUMBER);
        }
    }

    /* Encode the comb form. */
    for (i = 0; i < 4; i++)
        sysdata[i] = SET_PARITY(collect(sysdata[i]), PARITY_NUMBER);
}

/*
 * Set the memory address and the array length for a later disk access.
 * The unit and track numbers arrive later, with instruction 033 0023(0024).
 */
void disk_io(int ctlr, uint32 cmd)
{
    KMD *c     = &controller[ctlr];
    uint32 rem = cmd & ~(DISK_PAGE_MODE | DISK_PAGE | DISK_BLOCK | DISK_READ | DISK_READ_SYSDATA);
    if (rem && md_dev[ctlr * 4].dctrl & DEB_RWR) {
        besm6_debug("::: disk ctlr %c: unknown bits in IO request %08o", ctlr + '3', rem);
    }
    c->op     = cmd;
    c->format = 0;
    if (c->op & DISK_PAGE_MODE) {
        /* Page transfer */
        c->memory = (cmd & DISK_PAGE) >> 2 | (cmd & DISK_BLOCK) >> 8;
    } else {
        /* Half-page (track) transfer */
        c->memory = (cmd & (DISK_PAGE | DISK_HALFPAGE)) >> 2 | (cmd & DISK_BLOCK) >> 8;
    }
    if (md_dev[ctlr * 4].dctrl & DEB_RWR)
        besm6_debug("::: disk ctlr %c: request to %s %08o RAM @%05o", ctlr + '3',
                    (c->op & DISK_READ) ? "read" : "write", cmd, c->memory);
    disk_fail &= ~c->mask_fail;

    /* Clear the main interrupt register. */
    GRP &= ~c->mask_grp;
}

int has_debug(int ctlr)
{
    return (md_dev[ctlr * 4].dctrl & DEB_OPS) | (md_dev[ctlr * 4 + 1].dctrl & DEB_OPS) |
           (md_dev[ctlr * 4 + 2].dctrl & DEB_OPS) | (md_dev[ctlr * 4 + 3].dctrl & DEB_OPS);
}

/*
 * Disk control: instruction 00 033 0023(0024).
 */
t_stat disk_ctl(int ctlr, uint32 cmd)
{
    KMD *c  = &controller[ctlr];
    UNIT *u = c->dev < 0 ? &md_unit[0] : &md_unit[c->dev];

    if (cmd & BBIT(13) && (has_debug(ctlr) || (u && u->dptr->dctrl & DEB_OPS))) {
        besm6_debug("::: disk ctlr %c: bit 13 + %04o", ctlr + '3', cmd & 07777);
    }

    if (cmd & BBIT(12)) {
        if (c->dev == -1)
            besm6_debug("Setting block address for unknown device");

        /* Hand the track address to the КМД.
         * The disk transfer is performed here as well.
         * The unit number is already known by this point. */
        if (!(u->flags & UNIT_ATT)) {
            /* Device not attached. */
            disk_fail |= c->mask_fail;
            return SCPE_OK;
        }
        if (IS_29MB(u)) {
            c->zone = ((cmd & BITS(11)) << 1) | (c->zone & 1);
        } else {
            c->zone  = (cmd >> 1) & BITS(10);
            c->track = cmd & 1;
        }

        if (u->dptr->dctrl & DEB_OPS) {
            if (IS_29MB(u))
                besm6_debug("::: disk ctlr %c: cmd %08o = setting track address %04o", ctlr + '3',
                            cmd, c->zone);
            else
                besm6_debug("::: disk ctlr %c: cmd %08o = setting track address %04o.%d",
                            ctlr + '3', cmd, c->zone, c->track);
        }
        disk_fail &= ~c->mask_fail;
        if (c->op & DISK_READ) {
            if (IS_29MB(u) || c->op & DISK_PAGE_MODE)
                CPU_TRY(disk_read(u));
            else
                CPU_TRY(disk_read_track(u));
        } else {
            if (u->flags & UNIT_RO) {
                /* Read only. */
                /*return SCPE_RO;*/
                disk_fail |= c->mask_fail;
                return SCPE_OK;
            }
            if (c->format)
                disk_format(u);
            else if (IS_29MB(u) || c->op & DISK_PAGE_MODE)
                CPU_TRY(disk_write(u));
            else
                CPU_TRY(disk_write_track(u));
        }

        /* Wait for an event from the device. */
        sim_activate(u, 20 * USEC); /* sped up for debugging */

    } else if (cmd & BBIT(11)) {
        /* Select a unit number and put it in the КМД mask register.
         * Bit 8 is unit 0, bit 7 is unit 1, ... bit 1 is unit 7.
         * Bit 9 is also set -- what does it mean? */
        if (cmd & BBIT(8))
            c->dev = 7;
        else if (cmd & BBIT(7))
            c->dev = 6;
        else if (cmd & BBIT(6))
            c->dev = 5;
        else if (cmd & BBIT(5))
            c->dev = 4;
        else if (cmd & BBIT(4))
            c->dev = 3;
        else if (cmd & BBIT(3))
            c->dev = 2;
        else if (cmd & BBIT(2))
            c->dev = 1;
        else if (cmd & BBIT(1))
            c->dev = 0;
        else if (cmd != BBIT(11)) {
            /* A bad unit selection mask. */
            c->dev = -1;
            besm6_debug("Bad unit selection command %o", cmd);
            return SCPE_OK;
        } else {
            c->dev = -1;
            return SCPE_OK;
        }
        c->dev += ctlr * 32 + c->group * 8;
        u = &md_unit[c->dev];
        if (IS_29MB(u)) {
            c->zone = (c->zone & ~1) | (cmd & BBIT(10) ? 1 : 0);
        }
        u = &md_unit[c->dev];

        if (u->dptr->dctrl & DEB_OPS)
            besm6_debug("::: disk ctlr %c: cmd = %08o, unit select %02o", ctlr + '3', cmd, c->dev);

        if (!(u->flags & UNIT_ATT)) {
            /* Device not attached. */
            disk_fail |= c->mask_fail;
            GRP &= ~c->mask_grp;
        }
        GRP |= c->mask_grp;

    } else if (cmd & BBIT(9)) {
        /* Group selection, LSB of track #, interrupt */
        if ((cmd & 01774) == 01400) {
            // Understood with or without bit 13
            c->group = cmd & 3;
            c->dev   = -1; // (c->dev & ~030) | (c->group << 3);
            // u = &md_unit[c->dev];
            if (has_debug(ctlr))
                besm6_debug("::: disk ctlr %c: selected group %d", ctlr + '3', c->group);
        }
        GRP |= c->mask_grp;
    } else if (cmd == 011050) {
        // Release the currently selected group (reset back to 0),
        // with no device selected
        c->group = 0;
        c->dev   = -1;
        if (has_debug(ctlr))
            besm6_debug("::: disk ctlr %c: reset group", ctlr + '3');
        GRP &= ~c->mask_grp;
        sim_activate(&md_unit[(c - controller) * 32], 20 * USEC); // any unit would do
    } else if (cmd & BBIT(8)) {
        besm6_debug("::: disk ctlr %c: cmd = %08o\n", ctlr + '3', cmd);
    } else {
        /* An instruction handed to the КМД. */
        switch (cmd & 077) {
        case 000: /* DISPAK issues this once at the start of the boot */
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: undocumented command %08o", ctlr + '3', cmd);
            break;
        case 001: /* seek to cylinder 0 */
#if 1
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: seek to cylinder 0", ctlr + '3');
#endif
            break;
        case 002: /* seek */
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: seek", ctlr + '3');
            break;
        case 003: /* read (НСМД to МОЗУ) */
        case 043: /* of a spare track */
#if 1
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: read", ctlr + '3');
#endif
            break;
        case 004: /* write (МОЗУ to НСМД) */
        case 044: /* of a spare track */
#if 1
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: write", ctlr + '3');
#endif
            break;
        case 005: /* format */
            c->format = 1;
            break;
        case 006: /* compare (МОЗУ against НСМД) */
#if 1
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: compare", ctlr + '3');
#endif
            break;
        case 007: /* read the header */
        case 047: /* of a spare track */
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: read %sheader", ctlr + '3',
                            cmd & 040 ? "spare " : "");
            disk_fail &= ~c->mask_fail;
            disk_read_header(u);

            /* Wait for an event from the device. */
            sim_activate(u, 20 * USEC); /* sped up for debugging */
            break;
        case 010: /* clear the status register */
#if 1
            if (has_debug(ctlr))
                besm6_debug("::: disk ctlr %c: clear status register", ctlr + '3');
#endif
            c->status = 0;
            break;
        case 011: /* poll bits 1-12 of the status register */
            if (c->dev == -1) {
                c->status = ~0;
                if (has_debug(ctlr)) {
                    besm6_debug("::: disk ctlr %c: status low req with no selection", ctlr + '3');
                }
                break;
            }
            c->status = 0;
            if (md_unit[c->dev].flags & UNIT_ATT)
                c->status = STATUS_READY;
#if 1
            if (u->dptr->dctrl & DEB_STA)
                besm6_debug("::: disk ctlr %c: poll low status bits - %04o", ctlr + '3', c->status);
#endif
            break;
        case 031: /* poll bits 13-24 of the status register */
            if (c->dev == -1) {
                if (has_debug(ctlr)) {
                    besm6_debug("::: disk ctlr %c: status high req with no selection", ctlr + '3');
                }
                break;
            }
            /* Always "no such unit": the old code tested UNIT_DISABLE, which
               marks a unit as detachable rather than detached, and every disk
               had it set -- so STATUS_POWERUP was never reported.  The
               behaviour is preserved. */
            c->status = STATUS_ABSENT;
            if (md_unit[c->dev].flags & UNIT_RO)
                c->status |= STATUS_READONLY;
            c->status >>= 12;
#if 1
            if (u->dptr->dctrl & DEB_STA)
                besm6_debug("::: disk ctlr %c: poll high status bits - %04o", ctlr + '3',
                            c->status);
#endif
            break;
        case 050: /* release the disk unit */
#if 1
            if (u->dptr->dctrl & DEB_OPS)
                besm6_debug("::: disk ctlr %c: release drive", ctlr + '3');
#endif
            break;
        default:
            besm6_debug("::: disk ctlr %c: unknown command %02o", ctlr + '3', cmd & 077);
            GRP |= c->mask_grp; /* so that it does not hang */
            break;
        }
    }
    return SCPE_OK;
}

/*
 * Query the controller status.
 */
int disk_state(int ctlr)
{
    KMD *c = &controller[ctlr];
    if (md_dev[ctlr * 4].dctrl & DEB_RRD || md_dev[ctlr * 4 + 1].dctrl & DEB_RRD ||
        md_dev[ctlr * 4 + 2].dctrl & DEB_RRD || md_dev[ctlr * 4 + 3].dctrl & DEB_RRD)
        besm6_debug("::: disk ctlr %c: row %d, poll status = %04o", ctlr + '3', c->group,
                    c->status);
    return c->status;
}

/*
 * Event: a disk transfer has finished.
 * Set the interrupt flag.
 */
t_stat disk_event(UNIT *u)
{
    KMD *c = unit_to_ctlr(u);

    GRP |= c->mask_grp;
    return SCPE_OK;
}

/*
 * Poll the transfer error register with instruction 033 4035.
 */
int disk_errors()
{
#if 0
    if (u->dptr->dctrl & DEB_RRD)
        besm6_debug ("::: disk ctlr: poll error register = %04o", disk_fail);
#endif
    return disk_fail;
}
