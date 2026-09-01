/*
 * Copyright (c) 1993-2000 by Alexander V. Lukyanov (lav@yars.free.net)
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

#include "block.h"
#include "colormnu.h"
#include "config.h"
#include "edit.h"
#include "epath.h"
#include "format.h"
#include "keymap.h"
#include "compat/cio.h"
#include "lesys.h"
#include "options.h"
#include "search.h"

Task<void> EditorReadKeymap()
{
    char filename[1024];
    FILE *f;

    /* Upstream probed keymap-$TERM first, twice. There is one terminal here. */
    snprintf(filename, sizeof(filename), "%s/.le/keymap", HOME);
    f = co_await b_fopen(filename, "r");
    if (f == NULL) {
        snprintf(filename, sizeof(filename), "%s/keymap", datadir);
        f = co_await b_fopen(filename, "r");
        if (f == NULL)
            co_return;
    }

    co_await ReadActionMap(f);
    /* The stream's own error, not errno: a parser that reached the end of a
       keymap has left errno set by whatever it last scanned. */
    if (b_ferror(f)) {
        FError(filename);
    }

    co_await b_fclose(f);
}

Task<void> LoadKeymapEmacs()
{
    static char kpath[LE_PATHMAX];
    const char *k = datafile(kpath, sizeof(kpath), "keymap-emacs");
    FILE *f       = co_await b_fopen(k, "r");
    if (!f) {
        FError(k);
        co_return;
    }
    co_await ReadActionMap(f);
    co_await b_fclose(f);
    RebuildKeyTree();
    co_await LoadMainMenu();
}
Task<void> LoadKeymapDefault()
{
    FreeActionCodeTable();
    ActionCodeTable = DefaultActionCodeTable;
    RebuildKeyTree();
    co_await LoadMainMenu();
    co_return;
}
Task<void> SaveKeymap()
{
    char filename[1024];
    FILE *f;

    snprintf(filename, sizeof(filename), "%s/.le/keymap", HOME);
    f = co_await b_fopen(filename, "w");
    if (!f) {
        FError(filename);
        co_return;
    }
    co_await WriteActionMap(f);
    co_await b_fclose(f);
}
