/*
 * Copyright (c) 1993-2015 by Alexander V. Lukyanov (lav@yars.free.net)
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
#ifdef HAVE_SYS_SYSLIMITS_H
#endif
#ifdef HAVE_SYS_PARAM_H
#endif
#ifdef HAVE_SYS_MOUNT_H
#endif
#ifdef HAVE_SYS_MMAN_H
#endif
#ifdef HAVE_UNISTD_H
#endif

#include "edit.h"
#include "leio.h"
#include "keymap.h"
#include "highli.h"
#ifdef HAVE_ALLOCA_H
#endif
#include "block.h"
#include "clipbrd.h"
#include "getch.h"
#include "bm.h"
#include "undo.h"

#ifndef MAP_FAILED
# define MAP_FAILED ((void*)-1)
#endif

#ifndef DISABLE_FILE_LOCKS
int    LockFile(int fd,bool drop)
{
   struct  flock   Lock;
   Lock.l_start=0;
   Lock.l_len=0;
   Lock.l_type=F_WRLCK;
   Lock.l_whence=SEEK_SET;

   if(fcntl(fd,F_SETLK,&Lock)==-1)
   {
      if(errno==EACCES || errno==EAGAIN)
      {
         struct flock   Lock1;
         char   msg[100];
         static  struct menu LockMenu[]={
         {" &Cancel ",MIDDLE-10,FDOWN-2},
         {"  &Wait  ",MIDDLE,FDOWN-2},
         {" &Ignore ",MIDDLE+10,FDOWN-2},
         {NULL}};
         static  struct menu LockMenu1[]={
         {" &Cancel ",MIDDLE-5,FDOWN-2},
         {"  &Wait  ",MIDDLE+5,FDOWN-2},
         {NULL}};
         struct  stat   st;
         Lock1=Lock;
         fcntl(fd,F_GETLK,&Lock1);
         if(Lock1.l_type==F_UNLCK)
         {
            co_return(-2);
         }
         co_await le_fstat(fd,&st);

         snprintf(msg,sizeof(msg),"This file is already locked by process %ld",(long)Lock1.l_pid);
         switch(ReadMenuBox(LockEnforce(st.st_mode)?
            LockMenu1:LockMenu,HORIZ,msg," Lock Error ",
	    VERIFY_WIN_ATTR,CURR_BUTTON_ATTR))
         {
         case(0):
         case('C'):
            co_return(-1);
         case('I'):
            co_return(0);
         case('W'):
            MessageSync("Waiting for unlocking the file... (C-x - cancel)");
            errno=EACCES;
            while(fcntl(fd,F_SETLK,&Lock)==-1 && (errno==EACCES || errno==EAGAIN))
            {
	       if(co_await WaitForKey(1000)!=ERR)
	       {
		  int action=co_await GetNextAction();
		  if(action==CANCEL)
                  {
                     ErrMsg("Interrupted by user");
                     co_return(-1);
                  }
               }
	       errno=0;
            }
            if(errno!=EACCES && errno!=EAGAIN)
               co_return(-2);
         }
      }
      else
      {
         co_return(-2);
      }
   }
   if(drop)
   {
      Lock.l_type=F_RDLCK;	// drop write lock to read lock
      fcntl(fd,F_SETLK,&Lock);
   }
   co_return(0);
}
#else /* DISABLE_FILE_LOCKS */
int   LockFile(int,bool)
{
   return 0;
}
#endif /* DISABLE_FILE_LOCKS */

struct  menu   ConCan4Menu[]={
{   " C&ontinue ",MIDDLE-6,FDOWN-2  },
{   "  &Cancel  ",MIDDLE+6,FDOWN-2  },
{NULL}};

off_t  GetDevSize(int fd)
{
#ifdef BLKGETSIZE
   unsigned sect=0;
   if(ioctl(fd,BLKGETSIZE,&sect)==0)
      co_return ((off_t)sect)<<9;
#endif

   off_t lower=0;
   off_t upper=0x10000;
   char buf[1024];

   for(;;)
   {
      off_t pos=co_await le_lseek(fd,upper,SEEK_SET);
      if(pos!=upper)
	 break;
      int res=co_await le_read(fd,buf,sizeof(buf));
      if(res<=0)
	 break;
      lower=upper;
      upper*=2;
   }
   for(;;)
   {
      if(upper<=lower)
	 break;
      off_t mid=(upper+lower)/2;
      off_t pos=co_await le_lseek(fd,mid,SEEK_SET);
      if(pos!=mid)
      {
	 upper=mid;
	 continue;
      }
      int res=co_await le_read(fd,buf,sizeof(buf));
      if(res>0)
	 lower=mid+res;
      else
	 upper=mid;
   }

   co_return upper;
}

const char *GetDefaultEol()
{
   const char *eol=getenv("LE_DEFAULT_EOL");
   if(!eol) {
#ifdef DEFAULT_EOL
      eol=DEFAULT_EOL;
#elif defined(__MSDOS__) || defined(__CYGWIN32__)
      return(EOL_DOS);
#else
      return(EOL_UNIX);
#endif
   }
   if(!strcmp(eol,"\\n") || !strcmp(eol,"NL"))
      return EOL_UNIX;
   if(!strcmp(eol,"\\r\\n") || !strcmp(eol,"CRNL"))
      return EOL_DOS;
   if(!strcmp(eol,"\\r") || !strcmp(eol,"CR"))
      return EOL_MAC;
   return eol;
}

int   LoadFile(char *name)
{
   struct stat    st;
   num    act_read;
   char   msg[256];
   const InodeInfo *old;

   CheckBlock();
   if(!hide)
      MainClipBoard.Copy();

   co_await EmptyText();
   ResetBookmarks();

   flag=REDISPLAY_ALL;

   errno=0;

   if(!name[0])
   {
      buffer_mmapped=false;
      co_return(OK);
   }

   snprintf(msg,sizeof(msg),"Loading the file \"%.60s\"...",name);
   MessageSync(msg);

   newfile=0;

   const char *open_name=name;

   if(co_await le_stat(open_name,&st)!=-1)
   {
      FileMode=st.st_mode;
      if((!buffer_mmapped && (S_ISBLK(FileMode) || S_ISCHR(FileMode)))
	 || S_ISFIFO(FileMode))
      {
	 ErrMsg("This is a special file or a pipe\nthat I cannot edit.");
	 co_await EmptyText();
	 co_return(ERR);
      }
      if(S_ISDIR(FileMode))
	 View|=TMP_RO_MODE;
   }
   else if(errno==ENOENT && !View && !buffer_mmapped)
   {
      int f=co_await le_open(open_name,O_CREAT|O_WRONLY|O_TRUNC,0644);
      if(f!=-1)
      {
	 co_await le_close(f);
	 newfile=1;
      }
      else
      {
	 ErrMsg("Cannot create the file.\n"
		"The directory does not exist or is not accessible\n"
	        "or does not permit writing");
	 co_await EmptyText();
	 co_return(ERR);
      }
   }
   int open_flags=View?O_RDONLY:O_RDWR;
   if(!View && !buffer_mmapped)
	open_flags|=O_CREAT;
   file=co_await le_open(open_name,open_flags,0664);
   if(file==-1 && !View)
   {
      View|=TMP_RO_MODE;
      file=co_await le_open(open_name,O_RDONLY);
	 /* try to open the file in read-only mode */
   }
   if(file==-1)
   {
      FError(open_name);
      co_await EmptyText();
      co_return(ERR);
   }

   // re-stat the file in case it was created
   co_await le_fstat(file,&st);
   FileMode=st.st_mode;

   if(!View)
   {
      int lock_res=LockFile(file,true);
      if(lock_res==-1)
      {
	 co_await le_close(file);
	 file=-1;
	 co_await EmptyText();
         co_return(ERR);
      }
      if(lock_res==-2)
	 ErrMsg("Warning: file locking failed");
   }

   if(!buffer_mmapped)
   {
      if(ReplaceTextFromFile(file,st.st_size,&act_read)!=OK)
      {
	 if(errno)
	    FError(name);
	 co_await EmptyText();
	 co_return(ERR);
      }
      CheckPoint();

      num DosLastLine=0;
      num UnixLastLine=0;
      num MacLastLine=0;
      CountNewLines(0,Size(),&UnixLastLine,&DosLastLine,&MacLastLine);

      if(UnixLastLine>MacLastLine*2 && UnixLastLine>DosLastLine*2) {
	 TextEnd=TextPoint(Size(),UnixLastLine,-1);
      } else if(MacLastLine>UnixLastLine*2 && MacLastLine>DosLastLine*2) {
	 SetEolStr(EOL_MAC);
	 TextPoint::OrFlags(COLUNDEFINED|LINEUNDEFINED);
	 TextEnd=TextPoint(Size(),MacLastLine,-1);
      } else if(DosLastLine>=UnixLastLine && DosLastLine>=MacLastLine && DosLastLine>0) {
	 SetEolStr(EOL_DOS);
	 TextPoint::OrFlags(COLUNDEFINED|LINEUNDEFINED);
	 TextEnd=TextPoint(Size(),DosLastLine,-1);
      } else {
	 // set default EOL
	 SetEolStr(GetDefaultEol());
	 TextPoint::OrFlags(COLUNDEFINED|LINEUNDEFINED);
      }
   }
   else /* buffer_mmapped */
   {
#ifdef HAVE_MMAP
      if((S_ISBLK(FileMode) || S_ISCHR(FileMode))
      && st.st_size<=0)
      {
	 // try to get device size
	 st.st_size=GetDevSize(file);
      }
      if(st.st_size>0)
      {
	 if(mmap_len==0) {
	    mmap_len=st.st_size;
	    if((off_t)mmap_len!=st.st_size) {
	       errno=ENOMEM;
	       FError(name);
	       co_await EmptyText();
	       co_return ERR;
	    }
	 }
	 buffer=(char*)mmap(0,mmap_len,PROT_READ|(View?0:PROT_WRITE),
			    MAP_SHARED,file,mmap_begin);
	 if(buffer==(char*)MAP_FAILED)
	 {
	    buffer=0;
	    FError(name);
	    co_await EmptyText();
	    co_return ERR;
	 }
	 BufferSize=mmap_len;
	 ptr1=ptr2=BufferSize;
	 GapSize=0;
	 TextEnd=TextPoint(BufferSize);
      }
#endif
   }

   modified=0;
   SetStdCol();

   hide=1;
   flag=REDISPLAY_ALL;

   co_await le_fstat(file,&st);
   FileInfo=InodeInfo(&st);
   strcpy(FileName,name);

   CurrentPos=TextBegin;
   if(SavePos)
   {
      old=PositionHistory.FindInode(FileInfo);
      if(old)
      {
	 if(old->offset!=-1)
	 {
	    CurrentPos=old->offset;
	    if(!in_hex_mode)
	       SetStdCol();
	 }
	 else if(old->line!=-1 && old->col!=-1)
	    MoveLineCol(old->line,old->col);
      }
   }

   LoadHistory+=HistoryLine(name);

   InitHighlight();

   ScrShift=0;
   CenterView();

   /* Upstream armed an alarm here; co_await AutoSaveTick() is asked instead, from
      Edit()'s loop between keystrokes. */
   co_return(OK);
}

int   MaxBackup=9;

static char *BackupName(char *buf,unsigned buf_size,char *bp,char *filename,char *bak,int n)
{
   unsigned nbytes=strlen(bak)+40+1;
   static char suffix[LE_PATHMAX];
   snprintf(suffix,nbytes,bak,n);
   snprintf(buf,buf_size,"%s/%s%s",bp,filename,suffix);
   return buf;
}

static void MoveBackup(char *bp,char *filename,char *bak,int n)
{
   static char bakname[LE_PATHMAX];

   BackupName(bakname,sizeof(bakname),bp,filename,bak,n);
   if(co_await le_access(bakname,F_OK)!=-1)
   {
      if(n>=MaxBackup)
	 co_await le_unlink(bakname);
      else
      {
         unsigned nbytes1=strlen(bp)+1+strlen(filename)+strlen(bak)+40+1;
	 static char bakname1[LE_PATHMAX];
	 BackupName(bakname1,nbytes1,bp,filename,bak,n+1);
	 if(!strcmp(bakname,bakname1))
	    co_await le_unlink(bakname);
	 else
	 {
	    MoveBackup(bp,filename,bak,n+1);
	    if(co_await le_rename(bakname,bakname1)==-1)
	       co_await le_unlink(bakname);
	 }
      }
   }
}

static Task<int> CreateBak(char *name)
{
   char  *buf2;
   num   buf2size;
   num   bytesread;
   struct stat st;
   int   fd,bfd;
   char  directory[256];
   char  *filename;
   int   namemax;
   int	 res=OK;

   filename=strrchr(name,'/');
   if(filename==NULL)
   {
      strcpy(directory,".");
      filename=name;
   }
   else
   {
      if(filename==name)
        strcpy(directory,"/");
      else
      {
        strncpy(directory,name,filename-name);
        directory[filename-name]=0;
      }
      filename++;
   }

   MessageSync("Creating backup file...");

   /* There is no pathconf; OPFS takes any name a filesystem would. */
   namemax=255;


   char *bp=BakPath;
   unsigned bp_size=sizeof(BakPath);
   if(*bp==0)
   {
      bp=directory;
      bp_size=sizeof(directory);
   }
   else if(bp[0]=='~' && (bp[1]==0 || isslash(bp[1])))
   {
      bp_size=strlen(bp)+strlen(HOME);
      static char bpbuf[LE_PATHMAX];
      bp=bpbuf;
      bp_size=sizeof(bpbuf);
      snprintf(bp,bp_size,"%s%s",HOME,BakPath+1);
   }

   MoveBackup(bp,filename,bak,1);

   unsigned nbytes=strlen(bp)+1+strlen(filename)+strlen(bak)+40+1;
   static char bakname[LE_PATHMAX];
   BackupName(bakname,sizeof(bakname),bp,filename,bak,1);

   if(co_await le_stat(name,&st)==-1)
   {
      FError(name);
      co_return ERR;
   }

   fd=co_await le_open(name,O_RDONLY);
   bfd=co_await le_open(bakname,O_TRUNC|O_CREAT|O_WRONLY,st.st_mode&~0077);

   if(fd==-1)
   {
      if(bfd!=-1)
         co_await le_close(bfd);
      FError(name);
      co_return ERR;
   }
   else if(bfd==-1)
   {
      co_await le_close(fd);
      FError(bakname);
      co_return ERR;
   }
   buf2size=st.st_size;
   if(buf2size>0x40000)
      buf2size=0x40000;
   if((buf2=(char*)malloc(buf2size))==NULL)
   {
      NotMemory();
      res=ERR;
   }
   else
   {
      num written=0;
      for(;;)
      {
         bytesread=co_await le_read(fd,buf2,buf2size);
         if(bytesread==-1)
         {
            FError(name);
            res=ERR;
	    break;
         }
         if(bytesread==0)
            break;
         if(write_loop(bfd,buf2,bytesread,&written)==ERR)
	 {
	    FError(bakname);
	    res=ERR;
	    break;
	 }
      }
      if(written!=st.st_size)
	 ErrMsg("File size has changed during the copying");
      free(buf2);
      buf2=NULL;
   }
   co_await le_close(fd);
   co_await le_close(bfd);

   /* Upstream gave the backup the original's mtime. There is no setter for
      one here -- touch_path moves a file to now and is the only thing that
      can -- so the backup carries the time it was written. */
   co_return res;
}

int CheckMode(mode_t mode)
{
   if((mode&S_IFMT)!=S_IFREG)
   {
     ErrMsg("This is not a regular file");
     return(0);
   }
   return(1);
}

Task<int>   SaveFile(char *name)
{
   struct stat st;
   char  msg[256];
   int   nfile;
   num   act_written;
   int   delete_old_file=0;

   if(buffer_mmapped)
   {
      if(!strcmp(name,FileName))
	 co_return OK;
   }

   if(Text && !View)
      co_await UserOptimizeText();

   snprintf(msg,sizeof(msg),"Saving the file \"%.60s\"...",name);
   MessageSync(msg);

   if(co_await le_stat(name,&st)!=-1)
   {
      if(!CheckMode(st.st_mode))
         co_return(ERR);

      InodeInfo   NewFileInfo(&st,GetLine(),GetCol());

      if(file!=-1)
      {
	 if(buffer_mmapped && FileInfo.SameFile(NewFileInfo))
	    co_return OK;

	 if(FileInfo.SameFileModified(NewFileInfo))
	 {
	    switch(ReadMenuBox(ConCan4Menu,HORIZ,"The file was changed out of the editor",
		     " Warning ",VERIFY_WIN_ATTR,CURR_BUTTON_ATTR))
	    {
	    case('C'):
	    case(0):
	       co_return(ERR);
	    }
	 }
	 else if(!FileInfo.SameFile(NewFileInfo))
	 {
	    switch(ReadMenuBox(ConCan4Menu,HORIZ,"The file already exists and will be overwritten",
		     " Verify ",VERIFY_WIN_ATTR,CURR_BUTTON_ATTR))
	    {
	    case('C'):
	    case(0):
	       co_return(ERR);
	    }
	    delete_old_file=1;
	 }
      }

      if(makebak && !newfile) /* only for 'old' files */
      {
	 if(co_await CreateBak(name)!=OK)
	 {
	    switch(ReadMenuBox(ConCan4Menu,HORIZ,"Cannot create backup file",
		     " Warning ",VERIFY_WIN_ATTR,CURR_BUTTON_ATTR))
	    {
	    case('C'):
	    case(0):
	       co_return(ERR);
	    }
	 }
      }
   }
   else
   {
     if(errno!=ENOENT)
     {
       FError(name);
       co_return(ERR);
     }
     st.st_mode=FileMode|0600;
     delete_old_file=1;
   }

   if(!newfile)
     delete_old_file=0;

   newfile=0;

   MessageSync(msg);

   errno=0;
   nfile=co_await le_open(name,O_CREAT|O_RDWR,st.st_mode);
   if(nfile==-1)
   {
     FError(name);
     co_return(ERR);
   }

   int lock_res=LockFile(nfile,false);
   if(lock_res==-1)
   {
     co_await le_close(nfile);
     co_return(ERR);
   }
   if(lock_res==-2)
      ErrMsg("Warning: file locking failed");

   // now after locking truncate the file
#ifdef HAVE_FTRUNCATE
   if (co_await le_ftruncate(nfile,0) < 0)
      /*ignore*/;
#else
   co_await le_close(co_await le_open(name,O_TRUNC|O_RDONLY));
#endif

   /* Upstream forced the new file to the source's mode. There are no
      permission bits in the filesystem here. */

   /* now, after all that stuff, write the buffer contents */
   errno=0;
   if(WriteBlock(nfile,0,Size(),&act_written)!=OK)
   {
     if(errno)
       FError(name);
     co_await le_close(nfile);
     co_return(ERR);
   }
   if(act_written!=Size())
   {
     ErrMsg("Cannot write the file up to end\nPerhaps disk is full");
     co_await le_close(nfile);
     co_return(ERR);
   }

   if(buffer_mmapped)
   {
      co_await le_close(nfile);
      co_return OK;
   }

   modified=0;
   CheckPoint();
   undo.FileSaved();

   co_await le_stat(name,&st);
   FileInfo=InodeInfo(&st);
   SavePosition();

   co_await le_close(file);
   file=nfile;
   LockFile(file,true);

   if(delete_old_file)
   {
     if(co_await le_stat(FileName,&st)!=-1 && st.st_size==0)
       co_await le_unlink(FileName);
   }

   if(FileName!=name)
      strcpy(FileName,name);

   co_return(OK);
}

Task<int>   ReopenRW()
{
   struct stat st;

   if(View==0)
      co_return(OK);

   if(co_await le_access(FileName,W_OK|R_OK)==-1)
   {
      if(co_await le_stat(FileName,&st)==-1)
      {
         FError(FileName);
         co_return ERR;
      }

      /* Upstream checked the owner and then chmod'ed the write bit on.
	 There is no owner and no bit: a file is writable or it is not. */
   }

   View=0;

   static char name[LE_PATHMAX];
   strcpy(name,FileName);

   offs oldbb=BlockBegin;
   offs oldbe=BlockEnd;
   offs oldpos=CurrentPos;
   int oldhide=hide;

   int res=LoadFile(name);
   if(res==OK)
   {
      BlockBegin=oldbb;
      BlockEnd=oldbe;
      CurrentPos=oldpos;
      hide=oldhide;
   }
   co_return res;
}

void SavePosition()
{
   num offset=CurrentPos;
   num line=CurrentPos.LineSimple();
   num col=CurrentPos.ColSimple();

   FileInfo.line=line;
   FileInfo.col=col;
   FileInfo.offset=offset;

   PositionHistory+=FileInfo;
}
