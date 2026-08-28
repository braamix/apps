/*
 * Copyright (c) 1993-2021 by Alexander V. Lukyanov (lav@yars.free.net)
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
#ifdef HAVE_UNISTD_H
#include "lesys.h"
#endif
#ifdef HAVE_ALLOCA_H
#endif
#include "edit.h"
#include "proc/io.h"
#include "proc/rt.h"
#include "proc/time.h"
#include "leio.h"
#include "block.h"
#include "keymap.h"
#include "clipbrd.h"
#include "getch.h"
#include "format.h"
#include "about.h"
#include "bm.h"
#include "undo.h"
#include "highli.h"

Task<void>  UserDeleteToEol()
{
   if(View || in_hex_mode)
      co_return;
   DeleteToEOL();
   if(!Text)
      SetStdCol();
   flag|=REDISPLAY_LINE;
}
Task<void>  UserDeleteLine()
{
   if(View || in_hex_mode)
      co_return;
   DeleteLine();
   flag|=REDISPLAY_AFTER;
}

Task<void>  UserLineUp()
{
   if(in_hex_mode)
   {
      CurrentPos-=16;
   }
   else
   {
      MoveUp();
   }
   co_return;
}
Task<void>  UserLineDown()
{
   if(in_hex_mode)
   {
      CurrentPos+=16;
   }
   else
   {
      num line=GetLine();
      MoveDown();
      if(line>=GetLine())
      {
	 if(Text)
	 {
	    if(!Bol())
	    {
	       num old_stdcol=SaveStdCol();
	       int old_modified=modified;
	       NewLine();
	       RestoreStdCol(old_stdcol);
	       modified=old_modified;
	    }
	 }
	 else
	    SetStdCol();
      }
   }
   co_return;
}

Task<void>  UserCharLeft()
{
   if(in_hex_mode)
   {
      if(ascii)
         MoveLeft();
      else
      {
         if(right)
            right=0;   /* shift cursor from the rigth hex digit to the left one */
         else
         {
            MoveLeft();
            right=1;
         }
      }
   }
   else
   {
      if(Text && Eol() && stdcol>GetCol())
      {
         stdcol--;
         co_return;
      }
      MoveLeftOverEOL();
   }
   SetStdCol();
}
Task<void>  UserCharRight()
{
   if(in_hex_mode)
   {
      if(ascii)
	 MoveRight();
      else
      {
         if(right)
         {
            MoveRight();
            right=0;
         }
         else
            right=1;
      }
   }
   else
   {
      if(Text && Eol())
      {
         AddStdCol(1);
      }
      else
      {
         MoveRightOverEOL();
         SetStdCol();
      }
   }
   co_return;
}

Task<void>  UserCopyFromDown()
{
   if(View || in_hex_mode)
      co_return;

   num   oc=GetCol();
   if(Text && stdcol>oc && Eol())
      oc=stdcol;

   TextPoint tp=CurrentPos;

   for(;;)
   {
      tp=TextPoint(tp.Line()+1,oc);
      if(EofAt(tp.Offset()))
         break;
      if(tp.Col()>oc)
      {
         PreUserEdit();
         InsertChar('\t');
	 SetStdCol();
         flag|=REDISPLAY_LINE;
         co_return;
      }
      if(tp.Col()==oc && !EolAt(tp.Offset()))
      {
         wchar_t ch=WCharAt(tp.Offset());
         PreUserEdit();
         if(insert)
            InsertWChar(ch);
         else
            ReplaceWCharExtMove(ch);
	 SetStdCol();
         flag|=REDISPLAY_LINE;
         co_return;
      }
   }
}
Task<void>  UserCopyFromUp()
{
   if(View || in_hex_mode)
      co_return;

   num   oc=GetCol();
   if(Text && stdcol>oc && Eol())
      oc=stdcol;

   TextPoint tp=CurrentPos;

   while(tp.Line()>0)
   {
      tp=TextPoint(tp.Line()-1,oc);
      if(tp.Col()>oc)
      {
         PreUserEdit();
         InsertChar('\t');
	 SetStdCol();
         flag|=REDISPLAY_LINE;
         co_return;
      }
      if(tp.Col()==oc && !EolAt(tp.Offset()))
      {
         wchar_t ch=WCharAt(tp.Offset());
         PreUserEdit();
         if(insert)
            InsertWChar(ch);
         else
            ReplaceWCharExtMove(ch);
	 SetStdCol();
         flag|=REDISPLAY_LINE;
         co_return;
      }
   }
}

Task<void>  UserDeleteBlock()
{
   if(DragMark)
      UserStopDragMark();

   if(View)
      co_return;
   CheckBlock();
   if(!hide)
   {
      flag=REDISPLAY_ALL;
      Delete();
   }
}
Task<void>  UserCopyBlock()
{
   if(DragMark)
      UserStopDragMark();

   if(View)
      co_return;
   CheckBlock();
   if(!hide)
   {
      flag=REDISPLAY_ALL;
      PreUserEdit();
      Copy();
   }
}
Task<void>  UserMoveBlock()
{
   if(DragMark)
      UserStopDragMark();

   if(View)
       co_return;
   CheckBlock();
   if(!hide)
   {
      flag=REDISPLAY_ALL;
      PreUserEdit();
      Move();
   }
}

Task<void>  UserBackwardDeleteWord()
{
   if(View || in_hex_mode)
      co_return;
   if(!IsAlNumLeft() && CharRel(-1)!=' ' && CharRel(-1)!='\t')
   {
      co_await UserBackSpace();
   }
   else
   {
      PreUserEdit();
      if(!Bol() && (CharRel(-1)==' ' || CharRel(-1)=='\t'))
      {
	 while(!Bol() && (CharRel(-1)==' ' || CharRel(-1)=='\t'))
	    DeleteBlock(1,0);
      }
      else
      {
	 while(!Bol() && IsAlNumLeft())
	    DeleteBlock(MBCharSize,0);
      }
   }
   SetStdCol();
   flag|=REDISPLAY_LINE;
}

Task<void>  UserForwardDeleteWord()
{
   if(View || in_hex_mode)
      co_return;
   if(!IsAlNumRel(0) && Char()!=' ' && Char()!='\t')
      co_await UserDeleteChar();
   else
   {
      PreUserEdit();
      if(!Eol() && (Char()==' ' || Char()=='\t'))
      {
	 while(!Eol() && (Char()==' ' || Char()=='\t'))
	    DeleteBlock(0,1);
      }
      else
      {
	 while(!Eol() && IsAlNumRel(0))
            DeleteBlock(0,MBCharSize);
      }
   }
   SetStdCol();
   flag|=REDISPLAY_LINE;
}

Task<void>  UserDeleteWord()
{
   if(View || in_hex_mode)
      co_return;
   if(!IsAlNumRel(0))
      co_await UserForwardDeleteWord();
   else
   {
      PreUserEdit();
      while(!Eol() && IsAlNumRel(0))
         DeleteBlock(0,MBCharSize);
      while(!Bol() && IsAlNumLeft())
	 DeleteBlock(MBCharSize,0);
   }
   SetStdCol();
   flag|=REDISPLAY_LINE;
}

void  UserMarkWord()
{
   if(DragMark)
      UserStopDragMark();

   offs word_begin=CurrentPos;
   offs word_end=CurrentPos;
   while(!BofAt(word_begin))
   {
      if(IsAlNumAt(word_begin-CharSizeLeftAt(word_begin)))
	 word_begin--;
      else
	 break;
   }
   while(!EofAt(word_end) && IsAlNumAt(word_end))
      word_end+=MBCharSize;
   if(word_end==word_begin)
      word_end+=MBCharSize;
   BlockBegin=word_begin;
   BlockEnd=word_end;
   hide=FALSE;
   flag=REDISPLAY_ALL;
}
Task<void>  UserMarkLine()
{
   if(DragMark)
      UserStopDragMark();

   BlockBegin=LineBegin(Offset());
   if(rblock)
      BlockEnd=BlockBegin;
   else
      BlockEnd=NextLine(Offset());
   hide=FALSE;
   flag=REDISPLAY_ALL;
   co_return;
}
Task<void>  UserMarkToEol()
{
   if(DragMark)
      UserStopDragMark();

   SetStdCol();
   BlockBegin=CurrentPos;
   BlockEnd=LineEnd(CurrentPos.Offset());
   hide=(BlockEnd.Col()<=BlockBegin.Col());
   flag=REDISPLAY_ALL;
   co_return;
}
Task<void>  UserMarkAll()
{
   if(DragMark)
      UserStopDragMark();

   BlockBegin=TextBegin;
   if(rblock)
      BlockEnd=LineBegin(TextEnd);
   else
      BlockEnd=TextEnd;
   hide=FALSE;
   flag=REDISPLAY_ALL;
   co_return;
}

Task<void>  UserPageTop()
{
   if(in_hex_mode)
   {
      if((CurrentPos&~15)==ScreenTop)
         CurrentPos-=(TextWinHeight-1)*16;
      else
         CurrentPos=ScreenTop+(CurrentPos&15);
   }
   else
   {
      if(Text)
      {
	 num oldstdcol=SaveStdCol();
         ToLineEnd();	// clear spaces at the line end
	 RestoreStdCol(oldstdcol);
      }

      if(GetLine()==ScreenTop.Line())
      {
	 CurrentPos=TextPoint(ScreenTop.Line()-(TextWinHeight-1),GetStdCol());
	 ScreenTop=LineBegin(CurrentPos);
	 flag=REDISPLAY_ALL;
      }
      else
	 CurrentPos=ScreenTop;
   }
   co_return;
}
void UserScrollUp()
{
   if(in_hex_mode) {
      ScreenTop-=16;
      if((CurrentPos-ScreenTop)/16>=TextWinHeight)
	 CurrentPos-=16;
   } else {
      ScreenTop=PrevLine(ScreenTop);
      if(GetLine()>=ScreenTop.Line()+TextWinHeight)
	 Task<co_await> UserLineUp();
   }
   flag=REDISPLAY_ALL;
}
Task<void> UserScrollDown()
{
   if(in_hex_mode) {
      if((TextEnd-ScreenTop)/16>=TextWinHeight) {
	 ScreenTop+=16;
	 if(CurrentPos<ScreenTop)
	    CurrentPos+=16;
      }
   } else {
      if(TextEnd.Line()-ScreenTop.Line()>=TextWinHeight) {
	 ScreenTop=NextLine(ScreenTop);
	 if(CurrentPos<ScreenTop)
	    co_await UserLineDown();
      }
   }
   flag=REDISPLAY_ALL;
}
Task<void>  UserPageUp()
{
   if(PreferPageTop)
   {
      co_await UserPageTop();
      co_return;
   }

   if(in_hex_mode)
   {
      int page_size=(TextWinHeight-1)*16;
      CurrentPos-=page_size;
      ScreenTop-=page_size;
   }
   else
   {
      num oldstdcol=SaveStdCol();
      if(Text)
	 ToLineEnd();
      CurrentPos=PrevNLines(CurrentPos,TextWinHeight-1);
      ScreenTop=PrevNLines(ScreenTop,TextWinHeight-1);
      RestoreStdCol(oldstdcol);
   }
   flag=REDISPLAY_ALL;
}
Task<void>  UserPageBottom()
{
   if(in_hex_mode)
   {
      int pgsize=(TextWinHeight-1)*16;
      if(CurrentPos>=ScreenTop+pgsize)
	 CurrentPos+=pgsize;
      else
	 CurrentPos+=pgsize-((CurrentPos&~15)-ScreenTop);
   }
   else
   {
      if(Text)
      {
	 num oldstdcol=SaveStdCol();
         ToLineEnd();
	 RestoreStdCol(oldstdcol);
      }

      if(GetLine()==ScreenTop.Line()+TextWinHeight-1)
      {
	 CurrentPos=TextPoint(GetLine()+TextWinHeight-1,GetStdCol());
	 ScreenTop=TextPoint(GetLine()-(TextWinHeight-1),0);
	 flag=REDISPLAY_ALL;
      }
      else
	 CurrentPos=TextPoint(ScreenTop.Line()+TextWinHeight-1,GetStdCol());
   }
   co_return;
}
Task<void>  UserPageDown()
{
   if(PreferPageTop)
   {
      co_await UserPageBottom();
      co_return;
   }

   if(in_hex_mode)
   {
      int page_size=(TextWinHeight*16-16);
      CurrentPos+=page_size;
      if(TextEnd-ScreenTop>=2*page_size)
	 ScreenTop+=page_size;
      else if(TextEnd>=page_size)
	 ScreenTop=(TextEnd-page_size)&~15;
   }
   else
   {
      num oldstdcol=SaveStdCol();

      if(Text)
	 ToLineEnd();

      CurrentPos=NextNLines(CurrentPos,TextWinHeight-1);
      if(TextEnd.Line()>=ScreenTop.Line()+2*TextWinHeight-2)
	 ScreenTop=NextNLines(ScreenTop,TextWinHeight-1);
      else
      {
	 offs NewScreenTop=PrevNLines(TextEnd,TextWinHeight-1);
	 if(NewScreenTop>ScreenTop)
	    ScreenTop=NewScreenTop;
      }

      RestoreStdCol(oldstdcol);
   }
   flag=REDISPLAY_ALL;
}

Task<void>  UserWordLeft()
{
   if(in_hex_mode && !ascii)
      MoveLeft();
   else
   {
      while(!Bof() && !IsAlNumLeft())
         CurrentPos-=MBCharSize;
      while(!Bof() && IsAlNumLeft())
         CurrentPos-=MBCharSize;
   }
   SetStdCol();
   co_return;
}
Task<void>  UserWordRight()
{
   if(in_hex_mode && !ascii)
      MoveRight();
   else
   {
      while(!Eof() && !IsAlNumRel(0))
         CurrentPos+=MBCharSize;
      while(!Eof() && IsAlNumRel(0))
         CurrentPos+=MBCharSize;
   }
   SetStdCol();
   co_return;
}

Task<void>  UserMenu()
{
   co_await ActivateMainMenu();
   co_return;
}

Task<void>  UserCommentLine()
{
   int unc=0;
   TextPoint   op=CurrentPos;

   if(View || in_hex_mode)
      co_return;

   ToLineBegin();
   if(Suffix(FileName,".cc")
   || Suffix(FileName,".cpp")
   || Suffix(FileName,".cxx")
   || Suffix(FileName,".java"))
   {
      if(BlockEq("//",2))
      {
	 DeleteBlock(0,2);
	 if(Char()==' ')
	    DeleteBlock(0,1);
      }
      else
      {
	 InsertBlock("// ",3);
      }
   }
   else if(Suffix(FileName,".sql"))
   {
      if(BlockEq("--",2))
      {
	 DeleteBlock(0,2);
	 if(Char()==' ')
	    DeleteBlock(0,1);
      }
      else
      {
	 InsertBlock("-- ",3);
      }
   }
   else if(Suffix(FileName,".c") || Suffix(FileName,".h")
   || Suffix(FileName,".css"))
   {
      if(BlockEq("//",2))
      {
	 DeleteBlock(0,2);
	 if(Char()==' ')
	    DeleteBlock(0,1);
	 goto done;
      }
      if(BlockEq("/*",2))
      {
	 unc=1;
	 DeleteBlock(0,2);
      }
      ToLineEnd();
      if(BlockEqLeft("*/",2))
      {
	 unc=1;
	 DeleteBlock(2,0);
      }
      if(!unc)
      {
	 InsertBlock("*/",2);
	 ToLineBegin();
	 InsertBlock("/*",2);
      }
   }
   else if(Suffix(FileName,".html") || Suffix(FileName,".htm")
   || Suffix(FileName,".shtml"))
   {
      if(BlockEq("<!--",4))
      {
	 unc=1;
	 DeleteBlock(0,4);
      }
      ToLineEnd();
      if(BlockEqLeft("-->",3))
      {
	 unc=1;
	 DeleteBlock(3,0);
      }
      if(!unc)
      {
	 InsertBlock("-->",3);
	 ToLineBegin();
	 InsertBlock("<!--",4);
      }
   }
   else // default
   {
      if(Char()=='#')
      {
	 DeleteChar();
	 if(Char()==' ')
	    DeleteChar();
      }
      else
      {
	 InsertBlock("# ",2);
      }
   }
done:
   CurrentPos=op;
   SetStdCol();
   flag|=REDISPLAY_LINE;
}

Task<void>  UserSetBlockBegin()
{
   PreUserEdit();
   flag=REDISPLAY_ALL;
   if(hide)
   {
      BlockBegin=BlockEnd=CurrentPos;
      hide=FALSE;
      co_return;
   }
   if(rblock?(CurrentPos.Line()<=BlockEnd.Line()
              && CurrentPos.Col()<=BlockEnd.Col())
            :(CurrentPos.Offset()<=BlockEnd.Offset()))
   //then
      BlockBegin=CurrentPos;
   else
   {
      BlockBegin=/*BlockEnd;*/
      BlockEnd=CurrentPos;
   }
   if(DragMark)
   {
      if(*DragMark < BlockBegin)
	 *DragMark = BlockBegin;
   }
}
Task<void>  UserSetBlockEnd()
{
   PreUserEdit();
   flag=REDISPLAY_ALL;
   if(hide)
   {
      BlockBegin=BlockEnd=CurrentPos;
      hide=FALSE;
      co_return;
   }
   if(rblock?(CurrentPos.Line()>=BlockBegin.Line()
              && CurrentPos.Col()>=BlockBegin.Col())
            :(CurrentPos.Offset()>=BlockBegin.Offset()))
   //then
      BlockEnd=CurrentPos;
   else
   {
      BlockEnd=/*BlockBegin;*/
      BlockBegin=CurrentPos;
   }
   if(DragMark)
   {
      if(*DragMark > BlockEnd)
	 *DragMark = BlockEnd;
   }
}

Task<void>  UserFindBlockBegin()
{
   if(hide)
      co_return;
   CurrentPos=BlockBegin;
   SetStdCol();
}
Task<void>  UserFindBlockEnd()
{
   if(hide)
      co_return;
   CurrentPos=BlockEnd;
   SetStdCol();
}

Task<void>  UserLineBegin()
{
   if(Text && !View)
      ToLineEnd();
   ToLineBegin();
   SetStdCol();
   co_return;
}
Task<void>  UserLineEnd()
{
   ToLineEnd();
   SetStdCol();
   if(autoindent && Text && Bol() && !Bof())
   {
      bool old_modified=modified;
      InsertAutoindent(TextPoint(CurrentPos-EolSize).Col());
      modified=old_modified;
   }
   co_return;
}
Task<void>  UserFileBegin()
{
   CurrentPos=TextBegin;
   SetStdCol();
   co_return;
}
Task<void>  UserFileEnd()
{
   CurrentPos=TextEnd;
   SetStdCol();
   co_return;
}

Task<void>  UserPreviousEdit()
{
   if(!modified)
      co_return;
   CurrentPos=ptr1;
   SetStdCol();
}

Task<void>  UserUnindent()
{
   num newmargin;
   num oldmargin;
   offs pos;
   int sz;
   num   curpos=GetCol();

   if(Text && curpos<stdcol && Eol())
      curpos=stdcol;

   pos=LineBegin(Offset());
   oldmargin=MarginSizeAt(pos);

   if(oldmargin==-1)
   {
      oldmargin=GetCol();
      if(Text && oldmargin<stdcol)
	 oldmargin=stdcol;
   }

   if(oldmargin!=curpos || oldmargin==0)
   {
      if(Text && Eol() && stdcol>GetCol())
      {
         co_await UserLineEnd();
	 co_return;
      }
      BackSpace();
   }
   else
   {
      for(;;)
      {
         pos=PrevLine(pos);
         newmargin=MarginSizeAt(pos);
         if(newmargin>=0 && newmargin<oldmargin)
            break;
         if(BofAt(pos))
         {
            newmargin=((oldmargin-1)/IndentSize)*IndentSize;
            break;
         }
      }
      if(Text && Eol())
      {
         DeleteToBOL();
         if(newmargin<curpos)
	    stdcol=newmargin;
	 else
	    stdcol=0;
	 flag|=REDISPLAY_LINE;
         co_return;
      }
      while(GetCol()>newmargin)
      {
         if(CharRel(-1)=='\t')
         {
            MoveLeft();
            if(newmargin<=GetCol())
               DeleteChar();
            else
            {
               sz=newmargin-GetCol();
               while(sz-->0)
                  InsertChar(' ');
               DeleteChar();
            }
         }
         else
            BackSpace();
      }
   }
   flag|=REDISPLAY_LINE;
   SetStdCol();
}

Task<void>  UserBackSpace()
{
   if(View)
      co_return;
   if(Bof() && (!Text || GetStdCol()==0))
      co_return;
   if(in_hex_mode)
   {
      BackSpace();
      flag|=REDISPLAY_AFTER;
      co_return;
   }
   if(Bol() && (!Text || GetStdCol()==0))
   {
      DeleteBlock(EolSize,0);
      flag|=REDISPLAY_AFTER;
   }
   else
   {
      if(!BackspaceUnindents)
      {
	 if(Text && Eol() && stdcol>GetCol())
	 {
	    //co_await UserLineEnd();
	    AddStdCol(-1);
	    co_return;
	 }
         BackSpace();
         flag|=REDISPLAY_LINE;
      }
      else
      {
         co_await UserUnindent();
         co_return;
      }
   }
   SetStdCol();
}

Task<void>  UserDeleteChar()
{
   if(View)
      co_return;
   if(Eof())
      co_return;
   if(in_hex_mode)
   {
      DeleteChar();
      flag=REDISPLAY_AFTER;
   }
   else
   {
      PreUserEdit();
      if(Eol())
      {
         DeleteEOL();
         flag=REDISPLAY_AFTER;
      }
      else
      {
         DeleteChar();
         flag=REDISPLAY_LINE;
      }
   }
   SetStdCol();
}

Task<int>   UserSave()
{
   if(FileName[0] && !View)
      co_return(co_await SaveFile(FileName));
   else
      co_return(co_await UserSaveAs());
}

int   file_check(const char *fn)
{
   char	 dir[256];
   char	 *slash;
   char	 msg[1024];

   if(co_await le_access(fn,R_OK)==-1)
   {
      if(co_await le_access(fn,F_OK)==0)
      {
	 snprintf(msg,sizeof(msg),"File: %s\nThe specified file is not readable",fn);
	 ErrMsg(msg);
	 co_return ERR;
      }
      if((View&RO_MODE) || buffer_mmapped)  // view mode or mmap mode
      {
	 snprintf(msg,sizeof(msg),"File: %s\nThe specified file does not exist",fn);
	 ErrMsg(msg);
	 co_return ERR;
      }
      strcpy(dir,fn);
      slash=dir+strlen(dir);
      while(slash>dir && !isslash(*--slash));
      if(slash>dir)
	 *slash=0;
      else
	 strcpy(dir,".");
      if(co_await le_access(dir,F_OK)==-1)
      {
	 snprintf(msg,sizeof(msg),"File: %s\nThe specified directory does not exist",fn);
	 ErrMsg(msg);
	 co_return ERR;
      }
      if(co_await le_access(dir,W_OK|X_OK)==-1)
      {
	 snprintf(msg,sizeof(msg),"File: %s\nThe specified file does not exist\n"
		"and the directory does not permit creating",fn);
	 ErrMsg(msg);
	 co_return ERR;
      }

      struct menu CreateOrNot[]=
      {
	 {" C&reate ",MIDDLE-6,4},
	 {" &Cancel ",MIDDLE+6,4},
	 {NULL}
      };
      snprintf(msg,sizeof(msg),"The file `%s' does not exist. Create?",fn);
      switch(ReadMenuBox(CreateOrNot,HORIZ,msg,
	 " Verify ",VERIFY_WIN_ATTR,CURR_BUTTON_ATTR))
      {
      case('R'):
	 co_return OK;
      default:
	 co_return ERR;
      }
   }
   co_return OK;
}

Task<void>    UserLoad()
{
   char  newname[256];
   newname[0]=0;

   if(getstring("Load: ",newname,sizeof(newname)-1,&LoadHistory)>0)
   {
      if(ChooseFileName(newname,sizeof(newname))<0)
         co_return;
      if(file_check(newname)==ERR)
      {
	 LoadHistory.Push();
	 co_return;
      }

      if(modified)
      {
         if(!AskToSave())
         {
            LoadHistory.Push();
            co_return;
         }
      }
      LoadFile(newname);
   }
}

Task<int>   UserSaveAs()
{
   char  newname[256];
   newname[0]=0;

   if(getstring("Save as: ",newname,sizeof(newname)-1,&LoadHistory,NULL,NULL)>0)
   {
      if(ChooseFileName(newname,sizeof(newname))<0)
         co_return(ERR);
      if(co_await SaveFile(newname)!=OK)
      {
         LoadHistory.Push();
         co_return(ERR);
      }
      co_return(OK);
   }
   co_return(ERR);
}
Task<void>  UserSwitch()
{
   LoadHistory.Open();
   LoadHistory.Prev();
   const HistoryLine *prev=LoadHistory.Prev();
   if(prev==NULL)
   {
      co_await UserLoad();
      co_return;
   }

   char newname[256];
   strncpy(newname,prev->get_line(),255);
   newname[255]=0;

   if(ChooseFileName(newname,sizeof(newname))<0)
      co_return;

   if(co_await le_access(newname,R_OK)==-1)
   {
      co_await UserLoad();
      co_return;
   }

   if(modified)
      if(!AskToSave())
         co_return;

   LoadHistory+=newname;

   LoadFile(newname);
}

Task<void>  UserInfo()
{
   WIN   *InfoWin;
   static char cwd[LE_PATHMAX];
   char  s[256];
   int   cl;

   DisplayWin(InfoWin=CreateWin(MIDDLE,MIDDLE,50,20,DIALOGUE_WIN_ATTR," Info ",0));

   /* There is no user and no group here; the owner line goes with them. */
   strcpy(cwd,"Unknown");
   {
      Result<String> d=Err(Error::NoMemory);
      if(Task<Result<String>> t=cwd_get())
	 d=co_await t;
      if(d.is_ok() && d.value().size()<sizeof(cwd))
      {
	 memcpy(cwd,d.value().data(),d.value().size());
	 cwd[d.value().size()]=0;
      }
   }

   do
   {
      snprintf(s,sizeof(s),"File: %.40s",FileName);
      PutStr(3,cl=2,s);

      snprintf(s,sizeof(s),"Line=%-6ld Col=%-6ld\nSize:%-6ld Offset:%-6ld",(long)GetLine(),
             (long)(Text&&Eol()?GetStdCol():GetCol()),(long)Size(),(long)Offset());
      PutStr(3,cl+=2,s);

      snprintf(s,sizeof(s),"CWD:  %.40s",cwd);
      PutStr(3,cl+=3,s);

      {
	 Civil c=civil((i64)(proc_now()/1000));
	 snprintf(s,sizeof(s),"Date: %04d-%02d-%02d %02d:%02d:%02d",
		  c.year,c.month,c.day,c.hour,c.min,c.sec);
      }
      PutStr(3,cl+=1,s);

      if(syntax_hl::selector) {
	 PutStr(3,cl+=2,"Syntax selector:");
	 PutStr(3,++cl,syntax_hl::selector);
      }

      refresh();
   }
   while(Task<co_await> co_await WaitForKey()==ERR);

   flushinp();

   CloseWin();
   DestroyWin(InfoWin);
   co_return;
}

Task<void>  UserToLineNumber()
{
   static char nl[10]="";
   if(getstring("Move to line: ",nl,sizeof(nl)-1,NULL,NULL,NULL)<1)
      co_return;
   GoToLineNum(strtol(nl,0,0)-1);
   SetStdCol();
}
Task<void>  UserToOffset()
{
   static char no[40]="";
   if(getstring("Move to offset: ",no,sizeof(no)-1,NULL,NULL,NULL)<1)
      co_return;
   CurrentPos=strtol(no,0,0);
   SetStdCol();
}

Task<void>  UserIndent()
{
   /* #### what exactly needs to be done when !insert ? */
   if(Text && stdcol>=GetCol() && Eol())
   {
      stdcol=(stdcol/IndentSize+1)*IndentSize;
      co_return;
   }
   num addcol=0;
   num newcol=(GetCol()/IndentSize+1)*IndentSize;
   offs ptr;
   for(ptr=Offset(); !EolAt(ptr) && (CharAt(ptr)==' ' || CharAt(ptr)=='\t'); ptr++);
   if(EolAt(ptr))
   {
      // space after cursor up to line end -- delete it
      DeleteBlock(0,ptr-Offset());
   }
   else if(insert)
   {
      // delete the space anyway, but remember how much needs to be reinserted
      addcol=TextPoint(ptr).Col()-GetCol();
      DeleteBlock(0,ptr-Offset());
   }
   if(Text && stdcol>=GetCol() && Eol())
   {
      stdcol=(stdcol/IndentSize+1)*IndentSize;
      co_return;
   }
   PreUserEdit();
   if(insert)
   {
      while(!Bol() && (CharRel(-1)==' ' || CharRel(-1)=='\t'))
	 BackSpace();
   }
   while(GetCol()<newcol)
   {
      if(insert || Eol())
      {
         if(UseTabs && Tabulate(GetCol())<=newcol)
            InsertChar('\t');
         else
            InsertChar(' ');
      }
      else
      {
          MoveRight();
      }
   }
   TextPoint old=CurrentPos;
   while(addcol>0)
   {
      if(UseTabs && addcol>=TabSize-GetCol()%TabSize)
      {
	 addcol-=TabSize-GetCol()%TabSize;
	 InsertChar('\t');
      }
      else
      {
	 addcol--;
	 InsertChar(' ');
      }
   }
   CurrentPos=old;
   if(insert)
      flag|=REDISPLAY_LINE;
   SetStdCol();
}

Task<void>  UserNewLine()
{
   if(View)
      co_return;

   if(autoindent)
      UserAutoindent();
   else
   {
      NewLine();
      SetStdCol();
      flag|=REDISPLAY_AFTER;
   }
}

void  UserAutoindent()
{
   if(View)
      return;

   num oldcol=GetCol();
   if(Text && Eol() && oldcol<stdcol)
      oldcol=stdcol;

   bool do_indent=true;

   if(MarginSizeAt(Offset())==-1)
      DeleteToBOL();
   else
   {
      offs ptr;
      for(ptr=Offset(); !EolAt(ptr) && (CharAt(ptr)==' ' || CharAt(ptr)=='\t'); ptr++)
	 ;
      if(EolAt(ptr))
         DeleteToEOL();
      else
         do_indent=false;
   }

   NewLine();
   SetStdCol();
   flag|=REDISPLAY_AFTER;
   if(do_indent)
      InsertAutoindent(oldcol);
}

Task<void>  UserUndelete()
{
   if(View)
      co_return;
   Undelete();
   flag=REDISPLAY_ALL;
   SetStdCol();
}
Task<void>  UserUndo()
{
   if(View)
      co_return;
   if(!undo.Enabled())
   {
      co_await UserUndelete();
      co_return;
   }
   undo.UndoGroup();
   flag=REDISPLAY_ALL;
}
Task<void>  UserRedo()
{
   if(View)
      co_return;
   undo.RedoGroup();
   flag=REDISPLAY_ALL;
   SetStdCol();
}
Task<void>  UserUndoStep()
{
   if(View)
      co_return;
   undo.UndoOne();
   flag=REDISPLAY_ALL;
}
void  UserRedoStep()
{
   if(View)
      return;
   undo.RedoOne();
   flag=REDISPLAY_ALL;
   SetStdCol();
}

void  UserInsertChar(char ch)
{
   if(View)
      co_return;
   if(Text && autoindent && ch=='}' && MarginSizeAt(Offset())==-1 && MarginSizeAt(PrevLine(Offset()))==stdcol)
   {
      const offs match = co_await FindMatch(ch);
      const num indent = match>=0 ? MarginSizeAt(match) : stdcol-IndentSize;
      DeleteToBOL();
      stdcol=indent;
   }
   PreUserEdit();
   InsertChar(ch);

   if(wordwrap)
      WordWrapInsertHook();

   if(in_hex_mode || Bol())
      flag|=REDISPLAY_AFTER;
   else
      flag|=REDISPLAY_LINE;
   SetStdCol();
}
void  UserReplaceChar(char ch)
{
   if(View)
      return;
   PreUserEdit();

   if(!in_hex_mode && Eol())
      flag|=REDISPLAY_AFTER;

   if(buffer_mmapped || in_hex_mode || !mb_mode)
      ReplaceCharMove(ch);
   else
   {
      InsertChar(ch);
      (void)MBCheckLeft();
      if(!MBCharInvalid)
	 DeleteChar();
   }

   if(!in_hex_mode && Bol())
      flag|=REDISPLAY_AFTER;
   else
      flag|=REDISPLAY_LINE;
   SetStdCol();
}

void  UserInsertControlChar(char ch)
{
   if(View)
      return;
   PreUserEdit();
   if((in_hex_mode && !insert) || buffer_mmapped)
   {
      if(!in_hex_mode && (Eol() || Char()=='\n'))
	 flag|=REDISPLAY_AFTER;
      ReplaceCharMove(ch);
      if(!in_hex_mode && Bol())
	 flag|=REDISPLAY_AFTER;
      else
	 flag|=REDISPLAY_LINE;
   }
   else
   {
      InsertChar(ch);
      if(in_hex_mode || Bol())
	 flag|=REDISPLAY_AFTER;
      else
	 flag|=REDISPLAY_LINE;
   }
   SetStdCol();
}
void  UserInsertString(const char *s,int len)
{
   while(len-->0)
      UserInsertControlChar(*s++);
}
void UserInsertWChar(wchar_t ch)
{
   char buf[MB_CUR_MAX+1];
   int len=wctomb(buf,ch);
   if(len<=0)
      return;
   UserInsertString(buf,len);
}

Task<void>  UserEnterControlChar()
{
   int   key;

   if(View)
      co_return;

   attrset(STATUS_LINE_ATTR->n_attr);
   mvaddch(StatusLineY,COLS-2,'^');
   SetCursor();
   key=co_await GetRawKey();
   if(key==ERR)
      co_return;
   UserInsertControlChar((char)key);
}

Task<void>  UserWordHelp()
{
   if(*GetWord())
      co_await cmd(HelpCmd,0,1);
   co_return;
}

Task<void>  UserKeysHelp()
{
   Help("MainHelp"," Help on Keys ");
   co_return;
}

Task<void>  UserAbout()
{
   ShowAbout();
   move(LINES-1,COLS-1);
   co_await GetNextAction();
   HideAbout();
   co_return;
}

Task<void>  UserRefreshScreen()
{
   reset_prog_mode();
   flushinp();
   RedisplayAll();
   refresh();
   co_return;
}

Task<void>  UserChooseChar()
{
   if(mb_mode && !in_hex_mode)
      co_await UserChooseWChar();
   else
      co_await UserChooseByte();
   co_return;
}
Task<void>  UserChooseByte()
{
   int   ch=co_await choose_ch();
   if(ch!=-1)
      UserInsertControlChar(ch);
   co_return;
}
Task<void>  UserChooseWChar()
{
   wchar_t ch=co_await choose_wch();
   if(ch!=-1)
      UserInsertWChar(ch);
   co_return;
}

Task<void>  UserInsertCharCode()
{
   if(mb_mode && !in_hex_mode)
      co_await UserInsertWCharCode();
   else
      co_await UserInsertByteCode();
   co_return;
}
Task<void>  UserInsertByteCode()
{
   if(View)
      co_return;
   int ch=getcode_char();
   if(ch!=-1)
      UserInsertControlChar(ch);
}
Task<void>  UserInsertWCharCode()
{
   wchar_t ch=getcode_wchar();
   if(ch!=-1)
      UserInsertWChar(ch);
   co_return;
}

static int base_editmode=-1;

Task<void>  UserSwitchInsertMode()
{
   insert=!insert;
   co_return;
}
Task<void>  UserSwitchHexMode()
{
   if(editmode==HEXM)
   {
      if(base_editmode==HEXM)
         editmode=EXACT;
      else
         editmode=base_editmode;
      SetStdCol();
   }
   else
   {
      base_editmode=editmode;
      editmode=HEXM;
   }
   if(editmode==-1)
      editmode=EXACT;
   flag=REDISPLAY_ALL;
   if(editmode==HEXM)
      ScreenTop=ScreenTop&~15;
   else
      ScreenTop=LineBegin(ScreenTop);
   co_return;
}
Task<void>  UserSwitchTextMode()
{
   if(editmode==TEXT)
   {
      if(base_editmode==TEXT)
         editmode=EXACT;
      else
         editmode=base_editmode;
   }
   else
   {
      base_editmode=editmode;
      editmode=TEXT;
   }
   if(editmode==-1)
      editmode=EXACT;
   flag=REDISPLAY_ALL;
   if(editmode==HEXM)
      ScreenTop=ScreenTop&~15;
   else
      ScreenTop=LineBegin(ScreenTop);
   co_return;
}

Task<void>  UserSwitchRussianMode()
{
   if(inputmode==RUSS)
      inputmode=LATIN;
   else
      inputmode=RUSS;
   co_return;
}
Task<void>  UserSwitchGraphMode()
{
   if(inputmode==GRAPH)
      inputmode=LATIN;
   else
      inputmode=GRAPH;
   co_return;
}
Task<void>  UserSwitchAutoindentMode()
{
   autoindent=!autoindent;
   co_return;
}

Task<void>  UserBlockPrefixIndent()
{
   if(View)
      co_return;

   if(DragMark)
      UserStopDragMark();

   if(!co_await GetActionArgument("Prefix: "))
      co_return;

   PrefixIndent(ActionArgument,ActionArgumentLen);
   flag=REDISPLAY_ALL;
}

History	 ShellHistory;
History	 PipeHistory;

Task<void>  UserShellCommand()
{
   if(!co_await GetActionArgument("Shell-Command: ",&ShellHistory))
      co_return;
   co_await cmd(ActionArgument,/*save*/false,/*pause*/true);
}

Task<void>  UserPipeBlock()
{
   if(DragMark)
      UserStopDragMark();

   CheckBlock();
   if(hide || rblock || View)
      co_return;

   const char *filter=co_await GetActionArgument("Pipe through: ",&PipeHistory);
   if(!filter)
      co_return;

   MessageSync("Piping...");

   co_await PipeBlock(filter,TRUE,TRUE);
   flag=REDISPLAY_ALL;
}

Task<void>  UserYankBlock()
{
   if(DragMark)
      UserStopDragMark();

   if(View)
      co_return;
   MainClipBoard.PasteAndMark();
   OptionallyConvertBlockNewLines("yanked");
   flag=REDISPLAY_ALL;
}

Task<void>  UserStartDragMark()
{
   if(DragMark)
   {
      UserStopDragMark();
      co_return;
   }
   PreUserEdit();
   DragMark=new TextPoint(CurrentPos);
   if(hide)
      co_await UserSetBlockBegin();
}
void  UserStopDragMark()
{
   if(!DragMark)
      return;
   delete DragMark;
   DragMark=0;
}

static TextPoint *mark_move_point;
static bool mark_move_top,mark_move_left;
static void pre_mark_move()
{
   if(hide)
   {
   was_hidden:
      flag=REDISPLAY_ALL;
      BlockEnd=BlockBegin=CurrentPos;
      mark_move_point=&BlockEnd;
      mark_move_top=mark_move_left=false;
      return;
   }
   if(rblock)
   {
      mark_move_top=(CurrentPos.Line()==BlockBegin.Line());
      if(!mark_move_top && CurrentPos.Line()!=BlockEnd.Line())
	 goto was_hidden;
      mark_move_left=(CurrentPos.Col()==BlockBegin.Col());
      if(!mark_move_left && CurrentPos.Col()!=BlockEnd.Col())
	 goto was_hidden;
   }
   else
   {
      if(CurrentPos==BlockBegin)
	 mark_move_point=&BlockBegin;
      else if(CurrentPos==BlockEnd)
	 mark_move_point=&BlockEnd;
      else
	 goto was_hidden;  // just do the same.
   }
}
static void post_mark_move()
{
   hide=false;
   PreUserEdit();
   if(rblock)
   {
      if(mark_move_left)
      {
	 if(mark_move_top)
	    BlockBegin=CurrentPos;
	 else
	 {
	    BlockBegin=TextPoint::ForcedLineCol(BlockBegin.Line(),CurrentPos.Col());
	    BlockEnd  =TextPoint::ForcedLineCol(CurrentPos.Line(),BlockEnd.Col());
	 }
      }
      else
      {
	 if(mark_move_top)
	 {
	    BlockBegin=TextPoint::ForcedLineCol(CurrentPos.Line(),BlockBegin.Col());
	    BlockEnd  =TextPoint::ForcedLineCol(BlockEnd.Line(),CurrentPos.Col());
	 }
	 else
	    BlockEnd=CurrentPos;
      }
      // swap the points if needed.
      if(BlockBegin.Col()>BlockEnd.Col() && BlockBegin.Line()>=BlockEnd.Line())
	 goto swap_marks;
      if(BlockBegin.Col()>BlockEnd.Col())
      {
	 TextPoint tmp=BlockBegin;
	 BlockBegin=TextPoint::ForcedLineCol(BlockBegin.Line(),BlockEnd.Col());
	 BlockEnd  =TextPoint::ForcedLineCol(BlockEnd.Line(),tmp.Col());
      }
      else if(BlockBegin.Line()>BlockEnd.Line())
      {
	 TextPoint tmp=BlockBegin;
	 BlockBegin=TextPoint::ForcedLineCol(BlockEnd.Line(),BlockBegin.Col());
	 BlockEnd  =TextPoint::ForcedLineCol(tmp.Line(),BlockEnd.Col());
      }
   }
   else
   {
      flag=REDISPLAY_ALL;
      *mark_move_point=CurrentPos;
      if(BlockBegin>BlockEnd)
      {
      swap_marks:
	 TextPoint tmp=BlockBegin;
	 BlockBegin=BlockEnd;
	 BlockEnd=tmp;
      }
   }
}

#define MarkMove(move)		   \
   Task<void> UserMark##move()	   \
   {				   \
      pre_mark_move();		   \
      hide=1;			   \
      co_await User##move();	   \
      SeekStdCol();		   \
      post_mark_move();		   \
   }
MarkMove(CharLeft);
MarkMove(CharRight);
MarkMove(WordLeft);
MarkMove(WordRight);
MarkMove(LineBegin);
MarkMove(LineEnd);
MarkMove(FileBegin);
MarkMove(FileEnd);
MarkMove(PageDown);
MarkMove(PageUp);
MarkMove(PageTop);
MarkMove(PageBottom);
MarkMove(LineUp);
MarkMove(LineDown);

Task<void> UserOptimizeText()
{
   if(View || buffer_mmapped)
      co_return;

   offs     ptr;
   TextPoint  tp=CurrentPos;

   MessageSync("Optimizing...");
   bool at_indent=true;
   num col=0;
   for(ptr=0; !EofAt(ptr); ptr++)
   {
      byte ch=CharAt_NoCheck(ptr);
      if(ch!=' ' && ch!='\t')
	 at_indent=false;
      if(at_indent && ch=='\t') {
	 // optimize indentation (space+tab) => (tab)
	 while(ptr>0 && CharAt_NoCheck(ptr-1)==' ') {
	    int spaces_in_the_tab=col%TabSize;
	    int spaces_to_remove=spaces_in_the_tab?spaces_in_the_tab:TabSize;
	    CurrentPos=ptr;
	    DeleteBlock(spaces_to_remove,0);
	    ptr-=spaces_to_remove;
	    col-=spaces_to_remove;
	    if(spaces_in_the_tab==0) {
	       // we need to insert a tab to compensate for the spaces
	       InsertChar('\t');
	       MoveLeft();
	    }
// 	    assert(col==GetCol());
// 	    assert(ptr==CurrentPos);
	 }
      }
      if(EolAt(ptr))
      {
         CurrentPos=ptr;
         while(!Bol() && (CharRel_NoCheck(-1)==' ' || CharRel_NoCheck(-1)=='\t'))
	 {
            BackSpace();
	    ptr--;
	 }
	 ptr+=EolSize-1; // skip EOL at once
	 at_indent=true;
	 col=0;
      } else {
	 if(ch=='\t')
	    col=Tabulate(col);
	 else
	    col++;
      }
   }
   CurrentPos=TextEnd;
   while(!Bof() && Bol() && BolAt(Offset()-EolSize))
      DeleteBlock(EolSize,0);
   if(!Bol())
      NewLine();

   CurrentPos=tp;
   SetStdCol();
   flag=REDISPLAY_ALL;
}

Task<void> UserRememberBlock()
{
   if(DragMark)
      UserStopDragMark();

   MainClipBoard.Copy();
   co_return;
}

Task<void> UserSetBookmark()
{
   Message("Mark: ");
   move(LINES-1,6);
   curs_set(1);
   int key=co_await GetRawKey();
   if(key<256 && key>=0)
      SetBookmark(key);
   else
      beep();
   ClearMessage();
   co_return;
}

Task<void> UserGoBookmark()
{
   Message("Go to mark: ");
   move(LINES-1,12);
   curs_set(1);
   int key=co_await GetRawKey();
   if(key<256 && key>=0)
      GoBookmark(key);
   else
      beep();
   ClearMessage();
   co_return;
}

#define S(n) Task<void> UserSetBookmark##n() { SetBookmark('0'+n); co_return; }
S(0) S(1) S(2) S(3) S(4) S(5) S(6) S(7) S(8) S(9)
#define G(n) Task<void> UserGoBookmark##n() { GoBookmark('0'+n); co_return; }
G(0) G(1) G(2) G(3) G(4) G(5) G(6) G(7) G(8) G(9)
