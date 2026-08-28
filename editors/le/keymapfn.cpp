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

#include "config.h"
#include "lesys.h"
#include "edit.h"
#include "lefile.h"
#include "epath.h"
#include "keymap.h"

#include "block.h"
#include "options.h"
#include "keymap.h"
#include "format.h"
#include "search.h"
#include "colormnu.h"

Task<void>  EditorReadKeymap()
{
   char  filename[1024];
   FILE  *f;

   snprintf(filename,sizeof(filename),"%s/.le/keymap-%s",HOME,TERM);
   f=co_await le_fopen(filename,false);
   if(f==NULL)
   {
      snprintf(filename,sizeof(filename),"%s/keymap-%s",datadir,TERM);
      f=co_await le_fopen(filename,false);
      if(f==NULL)
      {
         snprintf(filename,sizeof(filename),"%s/.le/keymap",HOME);
         f=co_await le_fopen(filename,false);
         if(f==NULL)
         {
            snprintf(filename,sizeof(filename),"%s/keymap",datadir);
            f=co_await le_fopen(filename,false);
            if(f==NULL)
               co_return;
         }
      }
   }

   errno=0;
   co_await ReadActionMap(f);
   if(errno)
   {
      FError(filename);
   }

   co_await le_fclose(f);
}

Task<void> LoadKeymapEmacs()
{
   static char kpath[LE_PATHMAX];
   const char *k=datafile(kpath,sizeof(kpath),"keymap-emacs");
   FILE *f=co_await le_fopen(k,false);
   if(!f)
   {
      FError(k);
      co_return;
   }
   co_await ReadActionMap(f);
   co_await le_fclose(f);
   RebuildKeyTree();
   co_await LoadMainMenu();
}
Task<void> LoadKeymapDefault()
{
   FreeActionCodeTable();
   ActionCodeTable=DefaultActionCodeTable;
   RebuildKeyTree();
   co_await LoadMainMenu();
   co_return;
}
Task<void> SaveKeymap()
{
   char  filename[1024];
   FILE  *f;

   snprintf(filename,sizeof(filename),"%s/.le/keymap",HOME);
   f=co_await le_fopen(filename,true);
   if(!f)
   {
      FError(filename);
      co_return;
   }
   co_await WriteActionMap(f);
   co_await le_fclose(f);
}
Task<void> SaveKeymapForTerminal()
{
   char  filename[1024];
   FILE  *f;

   snprintf(filename,sizeof(filename),"%s/.le/keymap-%s",HOME,TERM);
   f=co_await le_fopen(filename,true);
   if(!f)
   {
      FError(filename);
      co_return;
   }
   co_await WriteActionMap(f);
   co_await le_fclose(f);
}
