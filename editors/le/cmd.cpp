/*
 * Copyright (c) 1993-2017 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* Running a command with the screen given back.
 *
 * Upstream left curses, called system(), and came back. There is no system()
 * here: spawn() takes the descriptors as an argument and wait_child() answers
 * the status. What has to be got right is the order, and it is the shell's own
 * (sh/job.cpp): the screen goes back, then the keyboard, then the child is
 * spawned, then the console goes to it -- a full-screen program claims the
 * keyboard in its very first step, so a child racing us for it would lose.
 *
 * The [Press any key] prompt cannot go through the Grid: the diff would paint
 * over the output it exists to let you read. It is bytes on stdout, and one
 * key taken with a bare keys_claim.
 */

#include "config.h"
#include "kernel/sysabi.h"
#include "lesys.h"
#include "edit.h"
#include "cmd.h"
#include "getch.h"
#include "leio.h"

#include "proc/io.h"
#include "proc/rt.h"

char    Shell  [256]="exec $SHELL";
char    Make   [256]="exec make";
char    Run    [256]="exec make run";
char    Compile[256]="exec make \"$FNAME.o\"";
char    HelpCmd[256]="";	/* upstream ran PKGDATADIR/help */

/* The claims, in the order the shell wants them back. */
static Task<void> give_screen()
{
    if(Task<Result<void>> t=curses_flush())
       co_await t;
    if(Task<Result<Geometry>> t=screen_claim(false))
       co_await t;
    if(Task<Result<Geometry>> t=keys_claim(false))
       co_await t;
}

static Task<void> take_screen()
{
    if(Task<Result<Geometry>> t=keys_claim(true))
       co_await t;
    if(Task<Result<Geometry>> t=screen_claim(true))
       co_await t;
    /* The alternate screen comes back blank and the Grid still holds what was
       on it, so the diff would find nothing to send. */
    curses_resized();
}

/* [Press any key to continue], written as bytes and answered with one key.
   Neither can go through the screen: it belongs to the output just now. */
static Task<void> pause_key()
{
    if(Task<Result<void>> t=write_all(SYS_STDOUT,"[Press any key to continue]"))
       co_await t;
    if(Task<Result<Geometry>> t=keys_claim(true))
       co_await t;
    if(Task<Result<KeyPress>> t=key_read())
       co_await t;
    if(Task<Result<Geometry>> t=keys_claim(false))
       co_await t;
    if(Task<Result<void>> t=write_all(SYS_STDOUT,"\r\n"))
       co_await t;
}

/* cmd - execute command c */
Task<void>    cmd(const char *c,bool autosave,bool pauseafter)
{
    static char cl[1024];
    static char file[256],name[256],ext[256];
    const char  *s;
    char        *f,*n,*e,*p;
    extern struct menu ConCan4Menu[];
    int         exitcode;

    errno=0;
    if(modified && autosave)
    {
        co_await SaveFile(FileName);
        if(errno)
        {
            switch(co_await ReadMenuBox(ConCan4Menu,HORIZ,"Cannot save the file"," Warning ",
	       VERIFY_WIN_ATTR,CURR_BUTTON_ATTR))
            {
            case('C'):
            case(0):
                co_return;
            }
        }
    }
    /* Upstream cleared the file's lock-enforce bit here. There are no
       permission bits. */

    for(s=FileName,f=file,n=name,e=ext,p=NULL; *s; s++)
    {
        if(*s=='$' || *s=='`' || *s=='\\' || *s=='"')
            *e++ = *n++ = *f++ = '\\';
        if(*s=='.')
        {
            p=n;
            e=ext;
        }
        else
        {
            if(*s=='/')
                p=NULL,e=ext;
        }
        *e++ = *n++ = *f++ = *s;
    }
    *e = *f = '\0';
    if(p)
        *p='\0';    /* there was extension */
    else
        *n='\0',*ext='\0';  /* there was no extension */

    snprintf(cl,sizeof(cl),
             "FILE=\"%s\";FNAME=\"%s\";EXT=\"%s\";WORD=\"%s\";export WORD FILE EXT FNAME; %s",
             file,name,ext,GetWord(),c);

    co_await give_screen();

    {
        Str words[3];
        Args v;
        Result<u32> pid_r=Err(Error::NoMemory);

        words[0]=Str("/bin/sh",7);
        words[1]=Str("-c",2);
        words[2]=Str(cl,strlen(cl));
        v.v=Span<const Str>(words,3);

        if(Task<Result<u32>> t=spawn(v))
           pid_r=co_await t;
        if(pid_r.is_err())
        {
           exitcode=-1;
        }
        else
        {
           u32 child=pid_r.value();
           Result<Exited> w=Err(Error::NoMemory);

           /* The console goes to the child, so a ^C reaches it and not us. */
           if(Task<Result<void>> t=set_fg(child))
              co_await t;
           if(Task<Result<Exited>> t=wait_child(child))
              w=co_await t;
           if(Task<Result<void>> t=set_fg(0))
              co_await t;
           exitcode=w.is_err()?-1:(int)w.value().status;
        }
    }

    if(pauseafter || exitcode!=0)
       co_await pause_key();

    co_await take_screen();
    flushinp();
    flag=REDISPLAY_ALL;
}

Task<void>    DoMake()
{
    if(View)
        co_return;
    co_await cmd(Make,1,1);
}
Task<void>    DoShell()
{
    co_await cmd(Shell,0,0);
}
Task<void>    DoRun()
{
    if(View)
        co_return;
    co_await cmd(Run,1,1);
}
Task<void>    DoCompile()
{
    if(View)
        co_return;
    co_await cmd(Compile,1,1);
}
