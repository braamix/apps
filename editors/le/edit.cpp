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

/* edit.c : main editor loop */

#include "config.h"
#include "lesys.h"
#ifdef HAVE_UNISTD_H
#endif
#ifdef HAVE_LANGINFO_H
#endif
#include "about.h"
#include "block.h"
#include "calc.h"
#include "edit.h"
#include "epath.h"
#include "getch.h"
#include "keymap.h"
#include "lefile.h"
#include "leio.h"
#include "options.h"
#include "proc/io.h"
#include "screen.h"
#include "search.h"
#include "undo.h"
#ifdef WITH_MOUSE

#endif

#ifdef DISABLE_FILE_LOCKS
#define fcntl(x, y, z) (0)
#endif

#ifdef HAVE__NC_FREE_AND_EXIT
extern "C" {
extern void _nc_free_and_exit(int);
#define ExitProgram(code) _nc_free_and_exit(code)
};
#else
#define ExitProgram(code) exit(code)
#endif

#ifdef HAVE_ALLOCA_H
#endif

char Program[256];
int quitting;

extern const char MainHelp[];

char BakPath[256] = "";

char PosName[256] = "";
char HstName[256] = "";

int SavePos = 1;
int SaveHst = 1;

int View = FALSE;

char *HOME;

int UseColor           = 1;
int UseTabs            = 0;
int IndentSize         = 4;
int BackspaceUnindents = 1;
int PreferPageTop      = 0;

void GoToLineNum(num line_num)
{
    CurrentPos = TextPoint(line_num, 0);
    SetStdCol();
}

Global<History> g_CodeHistory;
#define CodeHistory (g_CodeHistory.get())
Task<long> getcode(const char *prompt)
{
    long i;
    static char ch[10];
    static bool getcode_active = false;

    if (getcode_active)
        co_return (-1);

    getcode_active = true;

    if (co_await getstring(prompt, ch, sizeof(ch) - 1, &CodeHistory) < 1) {
        getcode_active = false;
        co_return (-1);
    }
    getcode_active = false;
    i              = strtol(ch, 0, 0);
    co_return ((int)i);
}
Task<int> getcode_char()
{
    long ch = co_await getcode("Char: ");
    if (ch < 0 || ch >= 256)
        co_return -1;
    co_return (int) ch;
}
Task<wchar_t> getcode_wchar()
{
    long ch = co_await getcode("Wide Char: ");
    if (ch < 0)
        co_return -1;
    co_return (wchar_t) ch;
}

Task<void> ProcessDragMark()
{
    if (CurrentPos < *DragMark) {
        if (CurrentPos != BlockBegin || (Text && stdcol > GetCol())) {
            BlockEnd = *DragMark;
            co_await UserSetBlockBegin();
        }
    } else if (CurrentPos >= *DragMark) {
        if (CurrentPos != BlockEnd || (Text && stdcol > GetCol())) {
            BlockBegin = *DragMark;
            co_await UserSetBlockEnd();
        }
    }
}

Task<void> Edit()
{
    int key;
    int action;
    ActionProc proc;
    num old_num_of_lines = -1;

    while (!quitting) {
        SeekStdCol();
        if (in_hex_mode)
            flag |= REDISPLAY_LINE;

        if (!in_hex_mode && !View && old_num_of_lines != TextEnd.Line()) {
            flag |= REDISPLAY_AFTER;
            old_num_of_lines = TextEnd.Line();
        }

        if (DragMark)
            co_await ProcessDragMark();

        /* Whatever the last command recorded. Upstream put the box up from
           inside ErrMsg; this is the same box one turn later. */
        if (ErrorPending()) {
            co_await ShowPendingError();
            continue;
        }

        /* The autosave, which upstream drove from an alarm. */
        co_await AutoSaveTick();

        ClearMessage();
        SyncTextWin();
        SetCursor();

        action = co_await GetNextAction();

        if (action == WINDOW_RESIZE) {
            CorrectParameters();
            flag |= REDISPLAY_ALL;
            continue;
        }

        if (in_hex_mode) {
            ShowMatchPos = 0;
            RedisplayLine();
            ShowMatchPos = 1;
        }

#ifdef WITH_MOUSE
        if (action == MOUSE_ACTION) {
            MEVENT mev;
            if (getmouse(&mev) == ERR)
                continue;
            if (InTextWin(mev.y, mev.x))
                MouseInTextWin(mev);
            else if (InScrollBar(mev.y, mev.x))
                MouseInScrollBar(mev);
        }
#endif // WITH_MOUSE

        if (action == QUIT_EDITOR) {
            /* Nothing to cancel at the top level. ^X is the same action. */
            if (LastActionKey == K_ESCAPE)
                continue;
            co_await Quit();
            continue;
        }

        proc = GetActionProc(action);
        if (proc) {
            undo.BeginUndoGroup();
            co_await proc();
            undo.EndUndoGroup();
            continue;
        }
        if (action != NO_ACTION)
            continue;
        if (StringTypedLen > 1)
            continue;
        key = (byte)(StringTyped[0]);
        if (in_hex_mode && key == '\t') {
            ascii = !ascii;
            continue;
        }
        if (View)
            continue;
        if (!ascii && in_hex_mode) {
            if (key < 0 || key > 255)
                continue;
            if (isdigit(key))
                key -= '0';
            else {
                key = toupper(key);
                if (key >= 'A' && key <= 'F')
                    key -= 'A' - 0x0A;
                else {
                    UnrefKey(key);
                    continue;
                }
            }
            undo.BeginUndoGroup();
            if (((insert && !right) || Eof()) && !buffer_mmapped) {
                InsertChar(0);
                MoveLeft();
                flag |= REDISPLAY_AFTER;
            }
            if (right) {
                if (ReplaceCharMove((Char() & 0xF0) + key) == OK)
                    right = 0;
            } else {
                if (ReplaceChar((Char() & 0x0F) + (key << 4)) == OK)
                    right = 1;
            }
            flag |= REDISPLAY_LINE;
            undo.EndUndoGroup();
        } else {
            if (key > 255 || (key >= 0 && key < ' ' && key != '\n' && key != '\t')) {
                UnrefKey(key);
                continue;
            }
            key = ModifyKey(key);
            if (buffer_mmapped) {
                if (Eol())
                    flag |= REDISPLAY_AFTER;
                else
                    flag |= REDISPLAY_LINE;
                ReplaceCharMove(key);
                SetStdCol();
                continue;
            }
            if (in_hex_mode) {
                if (insert) {
                    InsertChar(key);
                    flag |= REDISPLAY_AFTER;
                } else {
                    ReplaceCharMove(key);
                    SetStdCol();
                    flag |= REDISPLAY_LINE;
                }
                continue;
            }
            undo.BeginUndoGroup();
            switch (key) {
            case ('\n'):
                co_await UserNewLine();
                break;
            case ('\t'):
                co_await UserIndent();
                break;
            default: /* not a newline and not a tab */
                if (insert || Eol() || (Char() == '\t' && Tabulate(GetCol()) != (GetCol() + 1)))
                    co_await UserInsertChar(key);
                else
                    UserReplaceChar(key);
                flag |= REDISPLAY_LINE;
            }
            undo.EndUndoGroup();
        }
    }
}
Task<void> Quit()
{
    if (co_await AskToSave())
        co_await Terminate();
    co_return;
}
Task<int> AskToSave()
{
    if (modified && !View) {
        static struct menu Menu[] = {
            { "   &Yes   " }, { "   &No   " }, { " &Cancel " }, { NULL }
        };
        int result = TRUE;

        switch (co_await ReadMenuBox(Menu, HORIZ, "The file has been modified. Save?", "",
                                     VERIFY_WIN_ATTR, CURR_BUTTON_ATTR)) {
        case ('Y'):
            errno  = 0;
            result = (co_await UserSave() == OK);
            if (!result && modified) {
                co_await UserSaveAs();
                result = !modified;
            }
            break;
        case (0):
        case ('C'):
            result = FALSE;
            break;
        case ('N'):
            result = TRUE;
        }
        co_return (result);
    }
    SavePosition();
    co_return (TRUE);
}

#if defined(NCURSES_VERSION) || defined(__NCURSES_H)
#define NCUR
#endif

Task<void> InitCurses()
{
    /* Upstream started curses over a terminal it had to name; here the screen
       and the keyboard are claimed with two syscalls, and re-entering is what
       a resize does. */
    if (Task<Result<void>> t = curses_open())
        co_await t;

    le_start_color();

    cbreak();
    noecho();
    nonl();
    meta(stdscr, TRUE);
    raw();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
}
void TermCurses()
{
    move(LINES - 1, 0);
    clrtoeol();
    refresh();
    endwin();
}

int optUseColor = -1;

Task<void> Initialize()
{
    FILE *f;

    /* Where the package's share directory is: PKGDATADIR was a compile-time
       string and this is a readlink. Everything that reads a data file -- the
       keymap, the colours, the menu, the syntax rules, the help -- needs it,
       so it comes first. */
    co_await epath_init();

    InitModifyKeyTables();
    initcalc();

#ifndef MSDOS
    static char filename[LE_PATHMAX];
    unsigned nbytes = sizeof(filename);
    snprintf(filename, nbytes, "%s/.le", HOME);
    co_await le_mkdir(filename, 0700);
    strcat(filename, "/tmp");
    co_await le_mkdir(filename, 0700);
    snprintf(HstName, sizeof(HstName), "%s/.le/history2", HOME);
#else
    snprintf(HstName, sizeof(HstName), "%s/le.hst", HOME);
#endif

    co_await InstallSignalHandlers();

    co_await InitCurses();

    co_await ReadConf();

    if (optUseColor != -1)
        UseColor = optUseColor;

    init_attrs();

    InitMenu();

    MessageSync("Loading history...");
    f = co_await le_fopen(HstName, false);
    if (f) {
        co_await PositionHistory.ReadFrom(f);
        co_await LoadHistory.ReadFrom(f);
        co_await SearchHistory.ReadFrom(f);
        co_await ShellHistory.ReadFrom(f);
        co_await PipeHistory.ReadFrom(f);
        co_await le_fclose(f);
    }

    co_await EditorReadKeymap();
    RebuildKeyTree();

    co_await LoadMainMenu();
}
/* Where le was started. It never chdirs, so one call at startup is the key the
   no-argument search below uses. */
static char StartDir[LE_PATHMAX];

/* Whether a load-history entry names a file in StartDir. An absolute entry is
   matched on its directory; a relative one belongs to whatever directory it
   resolves in, which is here. */
static Task<bool> here(const char *f)
{
    if (f[0] != '/')
        co_return co_await le_access(f, R_OK) != -1;

    const char *slash = strrchr(f, '/');
    usize n           = slash == f ? 1 : (usize)(slash - f);

    if (strlen(StartDir) != n || memcmp(f, StartDir, n))
        co_return false;
    co_return co_await le_access(f, R_OK) != -1;
}

Task<void> Terminate()
{
    FILE *f;

    co_await EmptyText();

    curs_set(1);

    if (strcmp(FileName, HstName)) {
        if (SaveHst) {
            MessageSync("Saving history...");
            /* Upstream held a lock across the read-merge-write, and rewound one
               descriptor to do it. There is no lock here, so the old file is read
               and then written over.

               The merge is what needs the old file; the write does not. Upstream
               opened O_RDWR|O_CREAT and got both from one descriptor -- with the
               write under `if (f)` there is no first run, so the file is never
               created and nothing is ever remembered. */
            f = co_await le_fopen(HstName, false);
            if (f) {
                InodeHistory oldPositionHistory;
                History oldLoadHistory;
                History oldSearchHistory;
                History oldShellHistory;
                History oldPipeHistory;

                co_await oldPositionHistory.ReadFrom(f);
                co_await oldLoadHistory.ReadFrom(f);
                co_await oldSearchHistory.ReadFrom(f);
                co_await oldShellHistory.ReadFrom(f);
                co_await oldPipeHistory.ReadFrom(f);
                PositionHistory.Merge(oldPositionHistory);
                LoadHistory.Merge(oldLoadHistory);
                SearchHistory.Merge(oldSearchHistory);
                ShellHistory.Merge(oldShellHistory);
                PipeHistory.Merge(oldPipeHistory);

                co_await le_fclose(f);
            }

            f = co_await le_fopen(HstName, true);
            if (f) {
                co_await PositionHistory.WriteTo(f);
                co_await LoadHistory.WriteTo(f);
                co_await SearchHistory.WriteTo(f);
                co_await ShellHistory.WriteTo(f);
                co_await PipeHistory.WriteTo(f);

                co_await le_fclose(f);
            }
        }
    }

    TermCurses();

    /* exit() cannot unwind a coroutine; Edit()'s loop reads this. */
    quitting = 1;
}

Task<void> PrintUsage(int arg)
{
    (void)arg;
    co_await File::stdout().write(
        "Usage: le [OPTIONS] [FILES...]\n"
        "\n"
        "-r  --read-only    permanent read only mode (view)\n"
        "-h  --hex-mode     start in hex mode\n"
        "-b  --black-white  black and white mode\n"
        "-c  --color        color mode\n"
        "    --config=FILE  use specified file instead of le.ini\n"
        "    --dump-keymap  dump default keymap to stdout and exit\n"
        "    --dump-colors  dump default color map to stdout and exit\n"
        "    --help         this description\n"
        "    --version      print LE version\n"
        "\n"
        "The last file will be loaded. If no files specified, the file last\n"
        "edited in this directory is reopened where it was left, and switch-file\n"
        "reaches the one before it.\n");
    co_await File::stdout().flush();
}

Task<i32> proc_main(Args args)
{
    int optView = -1, opteditmode = -1, optWarpLine = 0;
    int opt_use_mmap = -1;
    int opt;

    char newname[256];
    newname[0] = 0;

    /* getopt_long, by hand: five short options and eight long ones, which is
       smaller than carrying getopt. */
    static char argbuf[LE_PATHMAX];
    unsigned optind = 1;
    bool bad        = false;

    strncpy(Program, "le", sizeof(Program) - 1);

    HOME = getenv("HOME");
    if (HOME == NULL)
        HOME = (char *)"/home";

    /* Before Initialize(), which is where the history that keys off it loads. */
    {
        Result<String> d = Err(Error::NoMemory);

        if (Task<Result<String>> t = cwd_get())
            d = co_await t;
        if (d.is_ok() && d.value().size() < sizeof(StartDir)) {
            memcpy(StartDir, d.value().data(), d.value().size());
            StartDir[d.value().size()] = 0;
        }
    }

    for (; optind < args.size(); optind++) {
        Str a = args[optind];
        if (a.size() < 2 || a[0] != '-' || a == "--") {
            if (a == "--")
                optind++;
            break;
        }
        if (a[1] != '-') {
            for (usize i = 1; i < a.size(); i++) {
                switch (a[i]) {
                case 'r':
                    optView = 1;
                    break;
                case 'h':
                    opteditmode = HEXM;
                    break;
                case 'b':
                    optUseColor = 0;
                    break;
                case 'c':
                    optUseColor = 1;
                    break;
                default:
                    bad = true;
                    break;
                }
            }
            continue;
        }

        Str name = a.substr(2), val;
        usize eq = name.find('=');
        if (eq != Str::npos) {
            val  = name.substr(eq + 1);
            name = name.substr(0, eq);
        }

        if (name == "help") {
            co_await PrintUsage(0);
            co_return 0;
        } else if (name == "version") {
            co_await PrintVersion();
            co_return 0;
        } else if (name == "dump-keymap") {
            co_await WriteActionMap(&File::stdout());
            co_await File::stdout().flush();
            co_return 0;
        } else if (name == "dump-colors") {
            co_await DumpDefaultColors(&File::stdout());
            co_await File::stdout().flush();
            co_return 0;
        } else if (name == "read-only")
            optView = 1;
        else if (name == "hex-mode")
            opteditmode = HEXM;
        else if (name == "black-white")
            optUseColor = 0;
        else if (name == "color")
            optUseColor = 1;
        else if (name == "config") {
            ExplicitInitName = true;
            if (val.size() < sizeof(InitName)) {
                memcpy(InitName, val.data(), val.size());
                InitName[val.size()] = 0;
            }
        } else
            bad = true;
    }

    if (bad) {
        co_await File::stderr().write("le: unknown option; try `le --help'\n");
        co_return 1;
    }

    if (optUseColor != -1)
        UseColor = optUseColor;

    co_await Initialize();

    if (optView != -1)
        View = !!optView;
    if (opteditmode != -1)
        editmode = opteditmode;
    if (optUseColor != -1)
        UseColor = optUseColor;
    if (opt_use_mmap != -1)
        buffer_mmapped = opt_use_mmap;

    if (optind + 1 < args.size() && args[optind].size() > 1 && args[optind][0] == '+' &&
        isdigit((unsigned char)args[optind][1])) {
        usize used;
        optWarpLine = (int)scan_i64(args[optind].substr(1), used).value_or(0);
        optind++;
    }

    if (optind >= args.size()) {
        const HistoryLine *hl = 0;
        static char second[LE_PATHMAX];

        /* This directory's session: the two most recent files edited here, so a
           bare `le' picks up where it left off in this directory rather than
           wherever it was last used. Upstream had only the walk below, which
           takes relative entries after the first -- an implicit stand-in for
           the same thing that answers wrong as soon as an absolute path is
           typed. */
        second[0] = 0;
        if (StartDir[0]) {
            LoadHistory.Open();
            for (;;) {
                const HistoryLine *h = LoadHistory.Prev();
                if (!h)
                    break;
                const char *f = h->get_line();
                if (!*f || !co_await here(f))
                    continue;
                if (!newname[0]) {
                    snprintf(newname, sizeof(newname), "%s", f);
                    hl = h;
                } else {
                    snprintf(second, sizeof(second), "%s", f);
                    break;
                }
            }
        }

        LoadHistory.Open();
        bool first = true;
        while (!newname[0]) {
            hl = LoadHistory.Prev();
            if (!hl)
                break;
            const char *f = hl->get_line();
            if (*f && (first || f[0] != '/') && co_await le_access(f, R_OK) != -1) {
                strcpy(newname, f);
                break;
            }
            first = false;
        }

        /* The other file of the pair. LoadFile puts newname in front of it, so
           this is what switch-file steps back to. */
        if (second[0])
            LoadHistory += second;

        if (!hl) {
            ShowAbout();
            if (co_await getstring("Load: ", newname, 255, &LoadHistory, NULL, NULL) < 1 ||
                co_await ChooseFileName(newname, sizeof(newname)) < 0)
                co_await Terminate();
            HideAbout();
        }
    } else {
        for (usize i = optind; i < args.size(); i++) {
            snprintf(argbuf, sizeof(argbuf), "%.*s", (int)args[i].size(), args[i].data());
            LoadHistory += argbuf;
        }
        snprintf(newname, sizeof(newname), "%.*s", (int)args[args.size() - 1].size(),
                 args[args.size() - 1].data());
    }
    if (newname[0] && co_await file_check(newname) == ERR) {
        if (View || buffer_mmapped)
            co_await Terminate();
        newname[0] = 0;
    }
    if (co_await LoadFile(newname) != ERR) {
        if (optWarpLine > 0)
            GoToLineNum(optWarpLine - 1);
    }
    co_await Edit();
    co_await Terminate();
    co_return 0;
}
