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

/* The names a keymap file spells a named key with.
 *
 * Upstream looked these up in terminfo -- $kcub1 was whatever escape sequence
 * the terminal sent for Left -- and dug into ncurses' own TERMTYPE for the
 * extended ones. Braam sends a key and not a sequence, so a name is a code and
 * the table is the whole of it. `C-` and `S-` prefixes stack. */

#include "config.h"
#include "edit.h"
#include "getch.h"
#include "keynames.h"

static const struct { const char *name; int code; } KeyNameTable[]={
   {"Up",	K_UP},
   {"Down",	K_DOWN},
   {"Left",	K_LEFT},
   {"Right",	K_RIGHT},
   {"Home",	K_HOME},
   {"End",	K_END},
   {"PgUp",	K_PGUP},
   {"PgDn",	K_PGDN},
   {"Ins",	K_INSERT},
   {"Del",	K_DELETE},
   {"BackTab",	K_BACKTAB},
   {"F1",	K_F1},
   {"F2",	K_F2},
   {"F3",	K_F3},
   {"F4",	K_F4},
   {"F5",	K_F5},
   {"F6",	K_F6},
   {"F7",	K_F7},
   {"F8",	K_F8},
   {"F9",	K_F9},
   {"F10",	K_F10},
   {"F11",	K_F11},
   {"F12",	K_F12},
   {NULL,	0}
};

int FindKeyCode(const char *name)
{
   int mods=0;

   for(;;)
   {
      if(!strncasecmp(name,"C-",2))
	 mods|=K_CTRL,name+=2;
      else if(!strncasecmp(name,"S-",2))
	 mods|=K_SHIFT,name+=2;
      else
	 break;
   }
   for(int i=0; KeyNameTable[i].name; i++)
      if(!strcasecmp(name,KeyNameTable[i].name))
	 return KeyNameTable[i].code|mods;
   return 0;
}

const char *FindKeyName(int code)
{
   static char buf[32];
   char *store=buf;

   if(code&K_CTRL)
      *store++='C',*store++='-';
   if(code&K_SHIFT)
      *store++='S',*store++='-';
   *store=0;
   for(int i=0; KeyNameTable[i].name; i++)
      if(KeyNameTable[i].code==(code&~(K_CTRL|K_SHIFT)))
      {
	 strcpy(store,KeyNameTable[i].name);
	 return buf;
      }
   return NULL;
}
