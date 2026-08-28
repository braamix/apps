/*
 * Copyright (c) 1993-2012 by Alexander V. Lukyanov (lav@yars.free.net)
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

/* functions, invoked with the keyboard by the user */

Task<void>  UserDeleteToEol();
Task<void>  UserDeleteLine();
Task<void>  UserDeleteWord();
Task<void>  UserForwardDeleteWord();
Task<void>  UserBackwardDeleteWord();

Task<void>  UserCopyFromDown();
Task<void>  UserCopyFromUp();

Task<void>  UserDeleteBlock();
Task<void>  UserCopyBlock();
Task<void>  UserMoveBlock();
void  UserMarkWord();
Task<void>  UserMarkLine();
Task<void>  UserMarkToEol();
Task<void>  UserMarkAll();

Task<void>  UserLineUp();
Task<void>  UserLineDown();
void  UserScrollUp();
Task<void>  UserScrollDown();
Task<void>  UserCharLeft();
Task<void>  UserCharRight();
Task<void>  UserPageTop();
Task<void>  UserPageUp();
Task<void>  UserPageBottom();
Task<void>  UserPageDown();
Task<void>  UserWordLeft();
Task<void>  UserWordRight();
Task<void>  UserLineBegin();
Task<void>  UserLineEnd();

Task<void>  UserMarkCharLeft();
Task<void>  UserMarkCharRight();
Task<void>  UserMarkWordLeft();
Task<void>  UserMarkWordRight();
Task<void>  UserMarkLineBegin();
Task<void>  UserMarkLineEnd();
Task<void>  UserMarkFileBegin();
Task<void>  UserMarkFileEnd();
Task<void>  UserMarkPageDown();
Task<void>  UserMarkPageUp();
Task<void>  UserMarkPageTop();
Task<void>  UserMarkPageBottom();
Task<void>  UserMarkLineUp();
Task<void>  UserMarkLineDown();

Task<void>  UserMenu();

Task<void>  UserCommentLine();

Task<void>  UserSetBlockBegin();
Task<void>  UserSetBlockEnd();
Task<void>  UserFindBlockBegin();
Task<void>  UserFindBlockEnd();
Task<void>  UserPipeBlock();

Task<void>  UserFileBegin();
Task<void>  UserFileEnd();

Task<void>  UserPreviousEdit();

Task<void>  UserBackSpace();
Task<void>  UserDeleteChar();

Task<void>  UserLoad();
Task<int>   UserSave();
Task<void>  UserSwitch();
Task<int>   UserSaveAs();

Task<void>  UserInfo();

Task<void>  UserToLineNumber();
Task<void>  UserToOffset();

Task<void>  UserIndent();
Task<void>  UserUnindent();

void  UserAutoindent();
Task<void>  UserNewLine();

Task<void>  UserUndelete();
Task<void>  UserUndo();
Task<void>  UserRedo();
Task<void>  UserUndoStep();
void  UserRedoStep();

Task<void>  UserEnterControlChar();

Task<void>  UserWordHelp();
Task<void>  UserKeysHelp();
Task<void>  UserAbout();

Task<void>  UserRefreshScreen();

Task<void>  UserChooseChar();
Task<void>  UserChooseWChar();
Task<void>  UserChooseByte();
Task<void>  UserInsertCharCode();
Task<void>  UserInsertWCharCode();
Task<void>  UserInsertByteCode();

void  UserInsertChar(char ch);
void  UserInsertControlChar(char ch);
void  UserReplaceChar(char ch);

Task<void>  UserSwitchHexMode();
Task<void>  UserSwitchTextMode();
Task<void>  UserSwitchInsertMode();
Task<void>  UserSwitchAutoindentMode();
Task<void>  UserSwitchRussianMode();
Task<void>  UserSwitchGraphMode();

Task<void>  UserBlockPrefixIndent();

Task<void>  UserShellCommand();

Task<void>  UserYankBlock();
Task<void>  UserRememberBlock();

Task<void>  UserStartDragMark();
void  UserStopDragMark();

Task<void>  UserOptimizeText();

Task<void>  UserSetBookmark();
Task<void>  UserGoBookmark();

#define S(n) void UserSetBookmark##n();
S(0) S(1) S(2) S(3) S(4) S(5) S(6) S(7) S(8) S(9)
#undef S
#define G(n) void UserGoBookmark##n();
G(0) G(1) G(2) G(3) G(4) G(5) G(6) G(7) G(8) G(9)
#undef G

extern class History ShellHistory;
extern class History PipeHistory;
