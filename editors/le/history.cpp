/*
 * Copyright (c) 1993-1997 by Alexander V. Lukyanov (lav@yars.free.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include "lesys.h"
#include "kernel/alloc.h"
#include "edit.h"
#include "lefile.h"
#include "proc/rt.h"

static char *memdup(const char *s, int len)
{
    if (!s)
        return 0;
    char *mem = (char *)malloc(len + 1);
    if (mem) {
        memcpy(mem, s, len);
        mem[len] = 0;
    }
    return mem;
}

History::History()
{
    lines = (HistoryLine **)calloc(HISTORY_SIZE, sizeof(*lines));
    curr  = -1;
}
void History::Open()
{
    curr = -1;
}
const HistoryLine *History::Curr()
{
    if (curr == -1)
        return (NULL);
    else {
        if (!lines[curr]) {
            curr = -1;
            return (NULL);
        }
        return (lines[curr]);
    }
}
const HistoryLine *History::Prev()
{
    if (curr == -1)
        curr = 0;
    else {
        curr++;
        if (curr == HISTORY_SIZE)
            curr = -1;
    }
    return (Curr());
}
const HistoryLine *History::Next()
{
    if (curr == -1) {
        curr = HISTORY_SIZE;
        while (!lines[--curr] && curr > 0)
            ;
    } else
        curr--;
    return (Curr());
}
void History::operator+=(const HistoryLine &hl)
{
    int i;

    if (hl.len == 0)
        return;

    for (i = 0; i < HISTORY_SIZE - 1 && lines[i] && hl != *lines[i]; i++)
        ;
    heap_delete(lines[i]);
    for (; i > 0; i--)
        lines[i] = lines[i - 1];
    lines[0] = heap_new<HistoryLine>(hl);
}
void History::operator-=(const HistoryLine &hl)
{
    int i;

    if (hl.len == 0)
        return;

    for (i = 0; lines[i] && hl != *lines[i]; i++)
        if (i >= HISTORY_SIZE - 1)
            return;
    heap_delete(lines[i]);
    for (; i < HISTORY_SIZE - 1; i++)
        lines[i] = lines[i + 1];
    lines[i] = 0;
}
void History::Push()
{
    HistoryLine hl2(lines[2]);
    HistoryLine hl1(lines[1]);
    *this += hl2;
    *this += hl1;
}

HistoryLine::HistoryLine()
{
    len     = 0;
    line    = 0;
    cr_time = 0;
}
HistoryLine::HistoryLine(const HistoryLine &hl)
{
    len     = hl.len;
    line    = memdup(hl.line, len);
    cr_time = hl.cr_time;
}
HistoryLine::HistoryLine(const HistoryLine *hl)
{
    if (!hl) {
        len     = 0;
        line    = 0;
        cr_time = 0;
        return;
    }
    len     = hl->len;
    line    = memdup(hl->line, len);
    cr_time = hl->cr_time;
}
HistoryLine::HistoryLine(const char *s, unsigned short l)
{
    if (l == 0)
        l = strlen(s);
    len     = l;
    line    = memdup(s, l);
    cr_time = (time_t)(proc_now() / 1000);
}
const HistoryLine &HistoryLine::operator=(const HistoryLine &hl)
{
    len = hl.len;
    free(line);
    line    = memdup(hl.line, len);
    cr_time = hl.cr_time;
    return (hl);
}

void History::Merge(const History &add)
{
    HistoryLine *const *l1 = this->lines;
    HistoryLine *const *l2 = add.lines;
    History newh;

    int i = HISTORY_SIZE, j = HISTORY_SIZE;
    for (;;) {
        while (i > 0 && !l1[i - 1])
            i--;
        while (j > 0 && !l2[j - 1])
            j--;
        if (i == 0 && j == 0)
            break;
        if (j == 0 || (i != 0 && l1[i - 1]->cr_time < l2[j - 1]->cr_time))
            newh += l1[--i];
        else
            newh += l2[--j];
    }
    *this = newh;
}

Task<void> History::WriteTo(FILE *f)
{
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (!lines[i])
            break;
        else {
            char hdr[40];
            snprintf(hdr, sizeof(hdr), "%lu:%u:", (unsigned long)lines[i]->cr_time, lines[i]->len);
            co_await le_puts(hdr, f);
            co_await f->write(Str(lines[i]->line, lines[i]->len));
        }
        co_await le_putc('\n', f);
    }
    co_await le_puts("0:0:\n", f);
}
Task<void> History::ReadFrom(FILE *f)
{
    for (int i = 0; i < HISTORY_SIZE; i++) {
        unsigned len = 0;
        unsigned long cr_time;
        Result<u64> t1 = co_await f->scan_u64();
        bool ok1       = t1.is_ok() && (co_await f->scan_lit(':')).value_or(false);
        Result<u64> t2 = ok1 ? co_await f->scan_u64() : Result<u64>(Err(Error::Invalid));
        bool ok2       = t2.is_ok() && (co_await f->scan_lit(':')).value_or(false);
        cr_time        = ok1 ? (unsigned long)t1.value() : 0;
        len            = ok2 ? (unsigned)t2.value() : 0;
        if (!ok2) {
            /* Upstream complained on stderr about a malformed record. The screen
               is taken by then, so a bad history file is simply the end of it. */
            co_return;
        }
        if (len == 0) {
            co_await le_getc(f); // skip \n
            co_return;
        }
        char *line = (char *)malloc(len + 1);
        if (!line)
            co_return;
        if ((co_await f->read({ line, len })).value_or(0) != len) {
            free(line);
            co_return;
        }
        line[len]         = 0;
        lines[i]          = heap_new<HistoryLine>();
        lines[i]->cr_time = cr_time;
        lines[i]->line    = line;
        lines[i]->len     = len;
        co_await le_getc(f); // skip \n
    }
    /* The two fields, discarded: this only steps past a record. */
    co_await f->scan_u64();
    co_await f->scan_lit(':');
    co_await f->scan_u64();
    co_await f->scan_lit(':');
    co_await le_getc(f); // skip \n
}

InodeInfo::InodeInfo()
{
    time = size = device = inode = line = col = offset = 0;
}
InodeInfo::InodeInfo(struct stat *st, num l, num c, num o)
{
    time   = st->st_mtime;
    size   = st->st_size;
    device = st->st_dev;
    inode  = st->st_ino;
    line   = l;
    col    = c;
    offset = o;
}
int InodeInfo::SameFile(const InodeInfo &file) const
{
#ifndef BROKEN_INODES
    return (device == file.device && inode == file.inode);
#else
    return (time == file.time && size == file.size);
#endif
}
int InodeInfo::SameFileModified(const InodeInfo &file) const
{
    return (SameFile(file) && (size != file.size || time != file.time));
}

int InodeHistory::FindInodeIndex(const InodeInfo &file)
{
    HistoryLine f_line(file.to_string());
    for (int i = 0; i < HISTORY_SIZE; i++) {
        if (!lines[i])
            break;
        InodeInfo info(lines[i]);
        if (info.SameFile(file) && !info.SameFileModified(file))
            return (i);
    }
    return (-1);
}

const InodeInfo *InodeHistory::FindInode(const InodeInfo &file)
{
    static InodeInfo *info;
    heap_delete(info);
    info = 0;

    int i = FindInodeIndex(file);
    if (i != -1)
        return (info = heap_new<InodeInfo>(lines[i]));

    return (0);
}
void InodeHistory::operator+=(const InodeInfo &file)
{
    int i = FindInodeIndex(file);
    if (i != -1)
        *(History *)this -= *lines[i];
    *(History *)this += file.to_string();
}
InodeInfo::InodeInfo(const HistoryLine *f_line)
{
    /* Seven longs separated by commas, which was one %ld,%ld,... */
    long v[7] = { 0, 0, 0, 0, 0, 0, 0 };
    Str rest(f_line->get_line(), strlen(f_line->get_line()));
    for (int i = 0; i < 7 && !rest.empty(); i++) {
        usize used;
        Option<i64> got = scan_i64(rest, used);
        if (!got.has_value())
            break;
        v[i] = (long)got.value();
        rest = rest.substr(used);
        if (!rest.empty() && rest[0] == ',')
            rest = rest.substr(1);
    }
    this->inode = v[0], this->device = v[1], this->time = v[2], this->size = v[3];
    this->line = v[4], this->col = v[5], this->offset = v[6];
}
const char *InodeInfo::to_string() const
{
    static char s[80];
    snprintf(s, sizeof(s), "%ld,%ld,%ld,%ld,%ld,%ld,%ld", (long)inode, (long)device, (long)time,
             (long)size, (long)line, (long)col, (long)offset);
    return s;
}
