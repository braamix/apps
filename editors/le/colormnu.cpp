/*
 * Copyright (c) 1998 by Alexander V. Lukyanov (lav@yars.free.net)
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
#include "epath.h"
#include "leio.h"
#include "colormnu.h"
#include "options.h"
#ifdef HAVE_ALLOCA_H
#endif

Task<void> ColorsSaveToFile(const char *f)
{
   DescribeColors(bw_pal,color_pal);
   co_await SaveConfToFile(f,colors);
}

static const char *const colors_file="/.le/colors";

/* The path datafile() builds, at file scope: these are coroutines. */
static char cfile[LE_PATHMAX];
Task<void> ColorsSave()
{
   static char f[LE_PATHMAX];
   unsigned nbytes=sizeof(f);
   snprintf(f,nbytes,"%s%s",HOME,colors_file);
   co_await ColorsSaveToFile(f);
   co_return;
}

Task<void> ColorsSaveForTerminal()
{
   static char f[LE_PATHMAX];
   unsigned nbytes=sizeof(f);
   snprintf(f,nbytes,"%s%s-%s",HOME,colors_file,TERM);
   co_await ColorsSaveToFile(f);
   co_return;
}

Task<void> LoadColor(const char *f)
{
   if(co_await le_access(f,R_OK)==-1)
   {
      FError(f);
      co_return;
   }
   co_await ReadConfFromFile(f,colors,false);
   ParseColors();
   init_attrs();
   clearok(stdscr,1);
   flag=REDISPLAY_ALL;
}

Task<void> LoadColorDefault()
{
   memcpy(color_pal,default_color_pal,sizeof(default_color_pal));
   memcpy(bw_pal,default_bw_pal,sizeof(default_bw_pal));
   init_attrs();
#if !defined(NCURSES_VERSION_PATCH) || NCURSES_VERSION_PATCH<980627
   clearok(stdscr,1);
#endif
   flag=REDISPLAY_ALL;
   co_return;
}

Task<void> LoadColorDefaultBG()
{
   co_await LoadColor(datafile(cfile,sizeof(cfile),"/colors-defbg"+1));
   co_return;
}
Task<void> LoadColorBlue()
{
   co_await LoadColor(datafile(cfile,sizeof(cfile),"/colors-blue"+1));
   co_return;
}
Task<void> LoadColorBlack()
{
   co_await LoadColor(datafile(cfile,sizeof(cfile),"/colors-black"+1));
   co_return;
}
Task<void> LoadColorWhite()
{
   co_await LoadColor(datafile(cfile,sizeof(cfile),"/colors-white"+1));
   co_return;
}
Task<void> LoadColorGreen()
{
   co_await LoadColor(datafile(cfile,sizeof(cfile),"/colors-green"+1));
   co_return;
}
