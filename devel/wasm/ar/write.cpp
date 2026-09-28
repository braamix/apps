/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>

#include "ar.h"
#include "archive.h"
#include "emit.h"

#define _ARMAG_LEN        8                          /* length of ar magic string */
#define _ARHDR_LEN        60                         /* length of ar header */
#define _INIT_AS_CAP      128                        /* initial archive string table size */
#define _INIT_SYMOFF_CAP  (256 * (sizeof(uint64_t))) /* initial so table size */
#define _INIT_SYMNAME_CAP 1024                       /* initial sn table size */
#define _MAXNAMELEN_SVR4  15                         /* max member name length in svr4 variant */
#define _TRUNCATE_LEN     15                         /* number of bytes to keep for member name */

static void add_to_ar_str_table(struct bsdar *bsdar, const char *name);
static void add_to_ar_sym_table(struct bsdar *bsdar, const char *name);
static struct ar_obj *create_obj_from_file(struct bsdar *bsdar, const char *name, time_t mtime);
static void create_symtab_entry(struct bsdar *bsdar, const u8 *maddr, size_t size);
static void free_obj(struct bsdar *bsdar, struct ar_obj *obj);
static void insert_obj(struct bsdar *bsdar, struct ar_obj *obj, struct ar_obj *pos);
static void read_objs(struct bsdar *bsdar, const char *archive, int checkargv);
static void write_cleanup(struct bsdar *bsdar);
static void write_data(struct bsdar *bsdar, Emit &a, const void *buf, size_t s);
static void write_header(struct bsdar *bsdar, Emit &a, const char *name, time_t mtime, uid_t uid,
                         gid_t gid, mode_t md, size_t size);
static void write_objs(struct bsdar *bsdar);

/*
 * Create object from file, return created obj upon success, or NULL
 * when an error occurs or the member is not newer than existing
 * one while -u is specified.
 */
static struct ar_obj *create_obj_from_file(struct bsdar *bsdar, const char *name, time_t mtime)
{
    struct ar_obj *obj;
    struct ar_file *sb;
    const char *bname;

    if (name == NULL)
        return (NULL);

    obj = static_cast<struct ar_obj *>(malloc(sizeof(struct ar_obj)));
    if (obj == NULL) {
        bsdar_errc(bsdar, errno, "malloc failed");
        return (NULL);
    }
    /* Braam: the front end opened and read it already. */
    sb = ar_lookup(bsdar, name);
    if (sb->error != 0) {
        bsdar_warnc(bsdar, sb->error, "can't open file: %s", name);
        free(obj);
        return (NULL);
    }

    bname = bsdar_basename(name);
    if (bsdar->options & AR_TR && strlen(bname) > _TRUNCATE_LEN) {
        if ((obj->name = static_cast<char *>(malloc(_TRUNCATE_LEN + 1))) == NULL) {
            bsdar_errc(bsdar, errno, "malloc failed");
            free(obj);
            return (NULL);
        }
        (void)strncpy(obj->name, bname, _TRUNCATE_LEN);
        obj->name[_TRUNCATE_LEN] = '\0';
    } else if ((obj->name = strdup(bname)) == NULL) {
        bsdar_errc(bsdar, errno, "strdup failed");
        free(obj);
        return (NULL);
    }

    if (!sb->regular) {
        bsdar_warnc(bsdar, 0, "%s is not an ordinary file", obj->name);
        goto giveup;
    }

    /*
     * When option '-u' is specified and member is not newer than the
     * existing one, the replacement will not happen. While if mtime == 0,
     * which indicates that this is to "replace a none exist member",
     * the replace will proceed regardless of '-u'.
     */
    if (mtime != 0 && bsdar->options & AR_U && sb->mtime <= mtime)
        goto giveup;

    /*
     * When option '-D' is specified, mtime and UID / GID from the file
     * will be replaced with 0, and file mode with 644. This ensures that
     * checksums will match for two archives containing the exact same
     * files.
     *
     * Braam: a file has no owner, group or mode, so those are always
     * the ones -D gives.
     */
    obj->uid = 0;
    obj->gid = 0;
    obj->md  = 0644;
    if (bsdar->options & AR_D)
        obj->mtime = 0;
    else
        obj->mtime = sb->mtime;
    obj->size  = sb->data.size();
    obj->maddr = obj->size != 0 ? sb->data.data() : NULL;

    return (obj);

giveup:
    free(obj->name);
    free(obj);
    return (NULL);
}

/*
 * Free object itself and its associated allocations.
 */
static void free_obj(struct bsdar *bsdar, struct ar_obj *obj)
{
    (void)bsdar;
    free(obj->name);
    free(obj);
}

/*
 * Insert obj to the tail, or before/after the pos obj.
 */
static void insert_obj(struct bsdar *bsdar, struct ar_obj *obj, struct ar_obj *pos)
{
    if (obj == NULL) {
        bsdar_errc(bsdar, 0, "try to insert a null obj");
        return;
    }

    if (pos == NULL || obj == pos)
        /*
         * If the object to move happens to be the position obj,
         * or if there is not a pos obj, move it to tail.
         */
        goto tail;

    if (bsdar->options & AR_B) {
        TAILQ_INSERT_BEFORE(pos, obj, objs);
        return;
    }
    if (bsdar->options & AR_A) {
        TAILQ_INSERT_AFTER(&bsdar->v_obj, pos, obj, objs);
        return;
    }

tail:
    TAILQ_INSERT_TAIL(&bsdar->v_obj, obj, objs);
}

/*
 * Read objects from archive into v_obj list. Note that checkargv is
 * set when read_objs is used to read objects from the target of
 * ADDLIB command (ar script mode), in this case argv array possibly
 * specifies the members ADDLIB want.
 */
static void read_objs(struct bsdar *bsdar, const char *archive, int checkargv)
{
    struct ar_file *f;
    struct ar_obj *obj;
    const char *bname;
    char *name;
    char **av;
    int i, find;
    Vec<Member> members;
    Vec<ArchiveSymbol> index;
    Out why;

    f = ar_lookup(bsdar, archive);
    if (f->error != 0) {
        bsdar_errc(bsdar, f->error, "Failed to open '%s'", archive);
        return;
    }
    if (!read_archive(Str(archive), f->data, members, index, why)) {
        ar_read_error(bsdar, f->data, why.s, 1);
        return;
    }
    for (const Member &m : members) {
        if ((name = strndup(m.name.data(), m.name.size())) == NULL) {
            bsdar_errc(bsdar, errno, "strdup failed");
            return;
        }

        /*
         * If checkargv is set, only read those members specified
         * in argv.
         */
        if (checkargv && bsdar->argc > 0) {
            find = 0;
            for (i = 0; i < bsdar->argc; i++) {
                av = &bsdar->argv[i];
                if (*av == NULL)
                    continue;
                bname = bsdar_basename(*av);
                if (strcmp(bname, name) != 0)
                    continue;

                *av  = NULL;
                find = 1;
                break;
            }
            if (!find) {
                free(name);
                continue;
            }
        }

        obj = static_cast<struct ar_obj *>(malloc(sizeof(struct ar_obj)));
        if (obj == NULL) {
            bsdar_errc(bsdar, errno, "malloc failed");
            free(name);
            return;
        }
        obj->name  = name;
        obj->size  = m.data.size();
        obj->maddr = obj->size != 0 ? m.data.data() : NULL;
        obj->uid   = m.uid;
        obj->gid   = m.gid;
        obj->md    = m.mode;
        obj->mtime = m.mtime;

        TAILQ_INSERT_TAIL(&bsdar->v_obj, obj, objs);
    }
}

/*
 * Determine the constitution of resulting archive.
 */
int ar_write_archive(struct bsdar *bsdar, int mode)
{
    struct ar_obj *nobj, *obj, *obj_temp, *pos;
    struct ar_file *sb;
    const char *bname;
    char **av;
    int exitcode, i;

    TAILQ_INIT(&bsdar->v_obj);
    exitcode = EXIT_SUCCESS;
    nobj     = NULL;
    pos      = NULL;

    /*
     * Test if the specified archive exists, to figure out
     * whether we are creating one here.
     */
    sb = ar_lookup(bsdar, bsdar->filename);
    if (sb->error != 0) {
        if (sb->error != ENOENT) {
            bsdar_warnc(bsdar, 0, "stat %s failed", bsdar->filename);
            return (EXIT_FAILURE);
        }

        /* We do not create archive in mode 'd', 'm' and 's'.  */
        if (mode != 'r' && mode != 'q') {
            bsdar_warnc(bsdar, 0, "%s: no such file", bsdar->filename);
            return (EXIT_FAILURE);
        }

        /* Issue a warning if -c is not specified when creating. */
        if (!(bsdar->options & AR_C))
            bsdar_warnc(bsdar, 0, "creating %s", bsdar->filename);
        goto new_archive;
    }

    /*
     * First read members from existing archive.
     */
    read_objs(bsdar, bsdar->filename, 0);
    if (bsdar->stopped)
        goto write_objs;

    /*
     * For mode 's', no member will be moved, deleted or replaced.
     */
    if (mode == 's')
        goto write_objs;

    /*
     * For mode 'q', we don't need to adjust existing members either.
     * Also, -a, -b and -i are ignored in this mode. New members are
     * always inserted at tail.
     */
    if (mode == 'q')
        goto new_archive;

    /*
     * Try to find the position member specified by user.
     */
    if (bsdar->options & AR_A || bsdar->options & AR_B) {
        TAILQ_FOREACH(obj, &bsdar->v_obj, objs)
        {
            if (strcmp(obj->name, bsdar->posarg) == 0) {
                pos = obj;
                break;
            }
        }

        /*
         * If can't find `pos' specified by user,
         * silently insert objects at tail.
         */
        if (pos == NULL)
            bsdar->options &= ~(AR_A | AR_B);
    }

    for (i = 0; i < bsdar->argc; i++) {
        av = &bsdar->argv[i];

        TAILQ_FOREACH_SAFE(obj, &bsdar->v_obj, objs, obj_temp)
        {
            bname = bsdar_basename(*av);
            if (bsdar->options & AR_TR) {
                if (strncmp(bname, obj->name, _TRUNCATE_LEN))
                    continue;
            } else if (strcmp(bname, obj->name) != 0)
                continue;

            if (mode == 'r') {
                /*
                 * if the new member is not qualified
                 * to replace the old one, skip it.
                 */
                nobj = create_obj_from_file(bsdar, *av, obj->mtime);
                if (nobj == NULL) {
                    exitcode = EXIT_FAILURE;
                    goto skip_obj;
                }
            }

            if (bsdar->options & AR_V)
                bsdar_printf(bsdar, 1, "%c - %s\n", mode, *av);

            TAILQ_REMOVE(&bsdar->v_obj, obj, objs);
            if (mode == 'd' || mode == 'r')
                free_obj(bsdar, obj);

            if (mode == 'm')
                insert_obj(bsdar, obj, pos);
            if (mode == 'r')
                insert_obj(bsdar, nobj, pos);

        skip_obj:
            *av = NULL;
            break;
        }
    }

new_archive:
    /*
     * When operating in mode 'r', directly add those user specified
     * objects which do not exist in current archive. When operating
     * in mode 'q', all objects specified in command line args are
     * appended to the archive, without comparing with existing ones.
     */
    for (i = 0; i < bsdar->argc; i++) {
        av = &bsdar->argv[i];
        if (*av != NULL && (mode == 'r' || mode == 'q')) {
            nobj = create_obj_from_file(bsdar, *av, 0);
            if (nobj == NULL) {
                exitcode = EXIT_FAILURE;
                *av      = NULL;
                continue;
            }
            insert_obj(bsdar, nobj, pos);
            if (bsdar->options & AR_V && nobj != NULL)
                bsdar_printf(bsdar, 1, "a - %s\n", *av);
            *av = NULL;
        }
    }

write_objs:
    if (!bsdar->stopped)
        write_objs(bsdar);
    write_cleanup(bsdar);

    return (exitcode);
}

/*
 * Memory cleaning up.
 */
static void write_cleanup(struct bsdar *bsdar)
{
    struct ar_obj *obj, *obj_temp;

    TAILQ_FOREACH_SAFE(obj, &bsdar->v_obj, objs, obj_temp)
    {
        TAILQ_REMOVE(&bsdar->v_obj, obj, objs);
        free_obj(bsdar, obj);
    }

    free(bsdar->as);
    free(bsdar->s_so);
    free(bsdar->s_sn);
    bsdar->as       = NULL;
    bsdar->s_so     = NULL;
    bsdar->s_so_max = 0;
    bsdar->s_sn     = NULL;
    /* Braam: and the counts, for ranlib's next archive. */
    bsdar->s_cnt   = 0;
    bsdar->s_sn_sz = 0;
    bsdar->as_sz   = 0;
}

/*
 * Wrapper for archive_write_data().
 */
static void write_data(struct bsdar *bsdar, Emit &a, const void *buf, size_t s)
{
    (void)bsdar;
    a.bytes(Bytes(static_cast<const u8 *>(buf), s));
}

/*
 * Braam: archive_write_header() of libarchive's svr4 ar format. A name
 * longer than 15 bytes is "/" and its offset into the archive string
 * table; "//" itself has only a size. Only the permission bits of a mode
 * are written, as llvm-ar writes them.
 */
static void write_header(struct bsdar *bsdar, Emit &a, const char *name, time_t mtime, uid_t uid,
                         gid_t gid, mode_t md, size_t size)
{
    char field[17];
    size_t len, at;

    len = strlen(name);
    if (strcmp(name, "/") == 0 || strcmp(name, "/SYM64/") == 0 || strcmp(name, "//") == 0)
        snprintf(field, sizeof(field), "%s", name);
    else if (len <= _MAXNAMELEN_SVR4)
        snprintf(field, sizeof(field), "%s/", name);
    else {
        /* The entry that is exactly this name. */
        for (at = 0; at + len + 2 <= bsdar->as_sz; at++)
            if ((at == 0 || bsdar->as[at - 1] == '\n') && strncmp(&bsdar->as[at], name, len) == 0 &&
                bsdar->as[at + len] == '/' && bsdar->as[at + len + 1] == '\n')
                break;
        snprintf(field, sizeof(field), "/%zu", at);
    }
    size_t start = a.v.size();
    emit_ar_header(a, Str(field), mtime, uid, gid, md & 07777, size);
    if (strcmp(name, "//") == 0 && !a.oom)
        for (at = 16; at < 48; at++)
            a.v[start + at] = ' ';
}

/*
 * Braam: a word of the symbol table, big-endian, `w_sz' bytes.
 */
static void write_word(Emit &a, uint64_t v, size_t w_sz)
{
    for (size_t i = w_sz; i-- > 0;)
        a.byte((u8)(v >> (8 * i)));
}

/*
 * Write the resulting archive members.
 */
static void write_objs(struct bsdar *bsdar)
{
    struct ar_obj *obj;
    size_t s_sz;  /* size of archive symbol table. */
    size_t pm_sz; /* size of pseudo members */
    size_t w_sz;  /* size of words in symbol table */
    size_t i;

    bsdar->rela_off = 0;

    /* Create archive symbol table and archive string table, if need. */
    TAILQ_FOREACH(obj, &bsdar->v_obj, objs)
    {
        if (!(bsdar->options & AR_SS) && obj->maddr != NULL)
            create_symtab_entry(bsdar, obj->maddr, obj->size);
        if (strlen(obj->name) > _MAXNAMELEN_SVR4)
            add_to_ar_str_table(bsdar, obj->name);
        bsdar->rela_off += _ARHDR_LEN + obj->size + obj->size % 2;
    }
    if (bsdar->stopped)
        return;

    /*
     * Pad the symbol name string table. It is treated specially because
     * symbol name table should be padded by a '\0', not the common '\n'
     * for other members. The size of sn table includes the pad bit.
     */
    if (bsdar->s_cnt != 0 && bsdar->s_sn_sz % 2 != 0)
        bsdar->s_sn[bsdar->s_sn_sz++] = '\0';

    /*
     * Archive string table is padded by a "\n" as the normal members.
     * The difference is that the size of archive string table counts
     * in the pad bit, while normal members' size fields do not.
     */
    if (bsdar->as != NULL && bsdar->as_sz % 2 != 0)
        bsdar->as[bsdar->as_sz++] = '\n';

    /*
     * If there is a symbol table, calculate the size of pseudo members,
     * convert previously stored relative offsets to absolute ones, and
     * then make them Big Endian.
     *
     * absolute_offset = htobe32(relative_offset + size_of_pseudo_members)
     *
     * Braam: write_word makes them big-endian as they are written.
     */
    w_sz = sizeof(uint32_t);
    if (bsdar->s_cnt != 0) {
        s_sz  = (bsdar->s_cnt + 1) * sizeof(uint32_t) + bsdar->s_sn_sz;
        pm_sz = _ARMAG_LEN + (_ARHDR_LEN + s_sz);
        if (bsdar->as != NULL)
            pm_sz += _ARHDR_LEN + bsdar->as_sz;
        /* Use the 64-bit word size format if necessary. */
        if (bsdar->s_so_max > UINT32_MAX - pm_sz) {
            w_sz = sizeof(uint64_t);
            pm_sz -= s_sz;
            s_sz = (bsdar->s_cnt + 1) * sizeof(uint64_t) + bsdar->s_sn_sz;
            pm_sz += s_sz;
        }
        for (i = 0; i < bsdar->s_cnt; i++)
            bsdar->s_so[i] += pm_sz;
    }

    ar_write w;
    w.path    = bsdar->filename;
    w.archive = true;
    Emit a{ w.image };

    write_data(bsdar, a, "!<arch>\n", _ARMAG_LEN);

    /*
     * write the archive symbol table, if there is one.
     * If options -s is explicitly specified or we are invoked
     * as ranlib, write the symbol table even if it is empty.
     *
     * Braam: its time is always 0, as llvm-ar writes it, so that -d and
     * -m are deterministic too.
     */
    if ((bsdar->s_cnt != 0 && !(bsdar->options & AR_SS)) || bsdar->options & AR_S) {
        write_header(bsdar, a, w_sz == sizeof(uint64_t) ? "/SYM64/" : "/", 0, 0, 0, 0,
                     (bsdar->s_cnt + 1) * w_sz + bsdar->s_sn_sz);
        write_word(a, bsdar->s_cnt, w_sz);
        for (i = 0; i < bsdar->s_cnt; i++)
            write_word(a, bsdar->s_so[i], w_sz);
        write_data(bsdar, a, bsdar->s_sn, bsdar->s_sn_sz);
    }

    /* write the archive string table, if any. */
    if (bsdar->as != NULL) {
        write_header(bsdar, a, "//", 0, 0, 0, 0, bsdar->as_sz);
        write_data(bsdar, a, bsdar->as, bsdar->as_sz);
    }

    /* write normal members. */
    TAILQ_FOREACH(obj, &bsdar->v_obj, objs)
    {
        write_header(bsdar, a, obj->name, obj->mtime, obj->uid, obj->gid, obj->md, obj->size);
        write_data(bsdar, a, obj->maddr, obj->size);
        if (obj->size % 2 != 0)
            a.byte('\n');
    }

    if (a.oom) {
        bsdar_errc(bsdar, ENOMEM, "%s", bsdar->filename);
        return;
    }
    w.data = Bytes(w.image.data(), w.image.size());
    bsdar->writes.push(move(w));
}

/*
 * Extract global symbols from ELF binary members.
 *
 * Braam: from a wasm object's linking section, in its order: the defined
 * symbols that are not local.
 */
static void create_symtab_entry(struct bsdar *bsdar, const u8 *maddr, size_t size)
{
    Vec<Str> names;
    char *name;

    if (!defined_symbols(Bytes(maddr, size), names)) {
        /* Silently ignore non-wasm member. */
        return;
    }
    for (Str n : names) {
        if ((name = strndup(n.data(), n.size())) == NULL) {
            bsdar_errc(bsdar, errno, "strdup failed");
            return;
        }
        add_to_ar_sym_table(bsdar, name);
        free(name);
    }
}

/*
 * Append to the archive string table buffer.
 */
static void add_to_ar_str_table(struct bsdar *bsdar, const char *name)
{
    if (bsdar->as == NULL) {
        bsdar->as_cap = _INIT_AS_CAP;
        bsdar->as_sz  = 0;
        if ((bsdar->as = static_cast<char *>(malloc(bsdar->as_cap))) == NULL) {
            bsdar_errc(bsdar, errno, "malloc failed");
            return;
        }
    }

    /*
     * The space required for holding one member name in as table includes:
     * strlen(name) + (1 for '/') + (1 for '\n') + (possibly 1 for padding).
     */
    while (bsdar->as_sz + strlen(name) + 3 > bsdar->as_cap) {
        bsdar->as_cap *= 2;
        bsdar->as = static_cast<char *>(realloc(bsdar->as, bsdar->as_cap));
        if (bsdar->as == NULL) {
            bsdar_errc(bsdar, errno, "realloc failed");
            return;
        }
    }
    strncpy(&bsdar->as[bsdar->as_sz], name, strlen(name));
    bsdar->as_sz += strlen(name);
    bsdar->as[bsdar->as_sz++] = '/';
    bsdar->as[bsdar->as_sz++] = '\n';
}

/*
 * Append to the archive symbol table buffer.
 */
static void add_to_ar_sym_table(struct bsdar *bsdar, const char *name)
{
    if (bsdar->s_so == NULL) {
        if ((bsdar->s_so = static_cast<uint64_t *>(malloc(_INIT_SYMOFF_CAP))) == NULL) {
            bsdar_errc(bsdar, errno, "malloc failed");
            return;
        }
        bsdar->s_so_cap = _INIT_SYMOFF_CAP;
        bsdar->s_cnt    = 0;
    }

    if (bsdar->s_sn == NULL) {
        if ((bsdar->s_sn = static_cast<char *>(malloc(_INIT_SYMNAME_CAP))) == NULL) {
            bsdar_errc(bsdar, errno, "malloc failed");
            return;
        }
        bsdar->s_sn_cap = _INIT_SYMNAME_CAP;
        bsdar->s_sn_sz  = 0;
    }

    if (bsdar->s_cnt * sizeof(uint64_t) >= bsdar->s_so_cap) {
        bsdar->s_so_cap *= 2;
        bsdar->s_so = static_cast<uint64_t *>(realloc(bsdar->s_so, bsdar->s_so_cap));
        if (bsdar->s_so == NULL) {
            bsdar_errc(bsdar, errno, "realloc failed");
            return;
        }
    }
    bsdar->s_so[bsdar->s_cnt] = bsdar->rela_off;
    if ((uint64_t)bsdar->rela_off > bsdar->s_so_max)
        bsdar->s_so_max = (uint64_t)bsdar->rela_off;
    bsdar->s_cnt++;

    /*
     * The space required for holding one symbol name in sn table includes:
     * strlen(name) + (1 for '\n') + (possibly 1 for padding).
     */
    while (bsdar->s_sn_sz + strlen(name) + 2 > bsdar->s_sn_cap) {
        bsdar->s_sn_cap *= 2;
        bsdar->s_sn = static_cast<char *>(realloc(bsdar->s_sn, bsdar->s_sn_cap));
        if (bsdar->s_sn == NULL) {
            bsdar_errc(bsdar, errno, "realloc failed");
            return;
        }
    }
    strncpy(&bsdar->s_sn[bsdar->s_sn_sz], name, strlen(name));
    bsdar->s_sn_sz += strlen(name);
    bsdar->s_sn[bsdar->s_sn_sz++] = '\0';
}
