/*
 * Copyright (c) 1993-2013 by Alexander V. Lukyanov (lav@yars.free.net)
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
#include "keymap.h"
#include "keynames.h"
#include "getch.h"

#include "block.h"
#include "options.h"
#include "keymap.h"
#include "format.h"
#include "search.h"
#include "colormnu.h"

unsigned char StringTyped[256];
int StringTypedLen;
int LastActionCode;
const char *ActionArgument;
int ActionArgumentLen;

int FuncKeysNum = 12;

int MouseCounter = 0;

const ActionNameProcRec ActionNameProcTable[] = {
#include "action-name-func.h"
};

const struct {
    const char *alias;
    int code;
} ActionNameAliases[] = { { "quit-editor", A_ESCAPE }, { 0 } };

enum { CODE_EQUAL, CODE_PREFIX, CODE_PAUSE, CODE_NOT_EQUAL, CODE_TOO_MUCH };

/* Upstream timed the gap between bytes, because an escape sequence and an ESC
   the user typed are the same two bytes and only the delay tells them apart.
   Braam sends a key rather than a sequence, so there is nothing to time and
   the node has no maxdelay. */
struct KeyTreeNode {
    int action;
    int keycode;
    struct KeyTreeNode *sibling;
    struct KeyTreeNode *child;
    const char *arg;
};

const ActionCodeRec *ActionCodeTable = DefaultActionCodeTable;
ActionCodeRec *DynamicActionCodeTable;

const char *GetActionName(int action)
{
    if (action >= A__FIRST && action <= A__LAST)
        return ActionNameProcTable[action - A__FIRST].name;
    return (NULL);
}

const char *GetActionCodeText(const char *code)
{
    static char code_text[1024];
    char *store         = code_text;
    unsigned store_size = sizeof(code_text);

    while (*code) {
        unsigned char the_code = *code++;
        if (iscntrl(the_code)) {
            if (the_code == '\033')
                snprintf(store, store_size, "\\e");
            else if (the_code < 32)
                snprintf(store, store_size, "^%c", the_code + '@');
            else
                snprintf(store, store_size, "\\%03o", the_code);

            unsigned nbytes = strlen(store);
            store += nbytes;
            store_size -= nbytes;
        } else {
            *(store++) = the_code;
            store_size -= 1;
        }
    }
    *store = 0;
    return (code_text);
}

#define LEFT_BRACE  '{'
#define RIGHT_BRACE '}'

/* The name after a $, with or without braces, leaving `c` on its last
   character. One reader for the three places that used to inline it. */
static void ScanKeyName(const char *&c, char *out, unsigned size)
{
    int bracket = (*c == LEFT_BRACE);
    char *store = out;

    c += bracket;
    while (*c != 0 && (bracket ? *c != RIGHT_BRACE : (isalnum((unsigned char)*c) || *c == '-')) &&
           (unsigned)(store - out) < size - 1)
        *store++ = *c++;
    *store = 0;
    if (!(bracket && *c == RIGHT_BRACE))
        c--;
}

static int PrettyCodeScore(const char *c)
{
    if (c == 0)
        return 1000000;

    int score = 0;
    while (*c) {
        score++;

        char term_name[256];
        char code_ch = *c;
        switch (code_ch) {
        case ('$'):
            code_ch = *(++c);

            if (code_ch == 0)
                break;

            ScanKeyName(c, term_name, sizeof(term_name));
            if (!FindKeyCode(term_name))
                return 1000000;
            score += 2;
            break;
        case ('|'):
            score += 5;
            break;
        case ('^'):
        case ('\\'):
            break;
        case ('\e'):
            if (c[1] == '[') // terminal codes are not pretty
                score += 6;
            break;
        }
        c++;
    }
    return score;
}

const char *ActionCodePrettyPrint(const char *c)
{
    static char code_text[1024];
    char *store         = code_text;
    unsigned store_size = sizeof(code_text);
    *store              = 0;

    while (*c) {
        char term_name[256];
        unsigned char code_ch = *c;
        switch (code_ch) {
        case ('$'):
            code_ch = *(++c);

            if (code_ch == 0)
                break;

            ScanKeyName(c, term_name, sizeof(term_name));
            {
                /* The name is already the pretty form: Left, C-Home, F4. */
                unsigned nbytes = snprintf(store, store_size, "%s", term_name);
                store += nbytes;
                store_size -= nbytes;
            }
            if (c[1] && c[1] != '|') {
                *store++ = ' ';
                *store   = 0;
            }
            break;
        case ('|'):
            *store++ = '+';
            *store   = 0;
            break;
        case ('^'):
            if (c[1]) {
                *store++ = '^';
                *store++ = toupper(*++c);
                *store   = 0;
                break;
            }
            goto default_l;
        case ('\\'):
            code_ch = *(++c);
        default:
        default_l:
            if (code_ch == 27 && c[1] == '|' && c[2] && c[2] != '$') {
                *store++ = 'M';
                *store++ = '-';
                *store   = 0;
                c++;
            } else if (code_ch < 32) {
                *store++ = '^';
                *store++ = code_ch + '@';
            } else if (code_ch == 128) {
                *store++ = '^';
                *store++ = '@';
            } else
                *store++ = code_ch;
            *store = 0;
        }
        c++;
    }
    return code_text;
}

const char *ShortcutPrettyPrint(int c, const char *arg)
{
    static char code_text[1024];
    char *store = code_text;

    const char *best_code = 0;
    int best_score        = 1000000;
    for (int i = 0; ActionCodeTable[i].action != -1; i++) {
        if (ActionCodeTable[i].action != c || xstrcmp(ActionCodeTable[i].arg, arg))
            continue;
        const char *code = ActionCodeTable[i].code;
        int score        = PrettyCodeScore(code);
        if (score < best_score) {
            best_code  = code;
            best_score = score;
        }
    }
    if (best_code == 0)
        return 0;

    strcpy(store, ActionCodePrettyPrint(best_code));
    return code_text;
}

Task<void> WriteActionMap(FILE *f)
{
    for (int i = 0; ActionCodeTable[i].action != -1; i++) {
        int pos            = 0;
        const char *a_name = GetActionName(ActionCodeTable[i].action);
        co_await le_puts(a_name, f);
        pos += strlen(a_name);
        const char *arg = ActionCodeTable[i].arg;
        if (arg) {
            co_await le_putc('(', f), pos++;
            while (*arg) {
                char out   = *arg;
                char bsout = 0;
                switch (*arg) {
                case '\n':
                    bsout = 'n';
                    break;
                case '\r':
                    bsout = 'r';
                    break;
                case '\t':
                    bsout = 't';
                    break;
                case '_':
                case '\\':
                    bsout = *arg;
                    break;
                case ' ':
                    out = '_';
                    break;
                }
                if (bsout)
                    co_await le_putc('\\', f), pos++;
                co_await le_putc(bsout ? bsout : out, f), pos++;
                arg++;
            }
            co_await le_putc(')', f), pos++;
        }
        co_await le_putc(' ', f), pos++;
        while (pos < 23)
            co_await le_putc(' ', f), pos++;
        co_await le_puts(GetActionCodeText(ActionCodeTable[i].code), f);
        co_await le_putc('\n', f);
    }
}

ActionProc GetActionProc(int action)
{
    if (action >= A__FIRST && action <= A__LAST)
        return ActionNameProcTable[action - A__FIRST].proc;
    return (NULL);
}

static KeyTreeNode *AddToKeyTree(KeyTreeNode *curr, int key_code, int action, const char *arg)
{
    KeyTreeNode *scan;
    for (scan = curr->child; scan; scan = scan->sibling)
        if (scan->keycode == key_code)
            break;
    if (!scan) {
        scan          = heap_new<KeyTreeNode>();
        scan->keycode = key_code;
        scan->action  = action;
        scan->arg     = arg;
        scan->child   = 0;
        scan->sibling = curr->child;
        curr->child   = scan;
    } else {
        if (scan->action == NO_ACTION) {
            scan->action = action;
            scan->arg    = arg;
        }
    }
    return (scan);
}

#define LEFT_BRACE  '{'
#define RIGHT_BRACE '}'

KeyTreeNode *BuildKeyTree(const ActionCodeRec *ac_table)
{
    KeyTreeNode *top = 0;
    char term_name[256];

    top          = heap_new<KeyTreeNode>();
    top->keycode = -1;
    top->action  = NO_ACTION;
    top->arg     = 0;
    top->child   = 0;
    top->sibling = 0;

    /* Upstream built each binding twice -- once against terminfo's escape
       sequence and once against the key name -- and walked every subset of the
       $names in a binding to do it. A name is one code here, so there is one
       pass and no fk_mask. */
    while (ac_table->action != -1) {
        KeyTreeNode *curr = top;
        const char *code  = ac_table->code;

        while (*code) {
            int key_code = 0;
            char code_ch = *code;

            switch (code_ch) {
            case ('$'):
                code_ch = *(++code);
                if (code_ch == 0)
                    break;
                ScanKeyName(code, term_name, sizeof(term_name));
                code++;
                key_code = FindKeyCode(term_name);
                if (!key_code) {
                    /* An unknown name binds nothing rather than binding NUL. */
                    while (*code && *code != '|')
                        code++;
                    continue;
                }
                break;
            case ('|'):
                code++;
                continue;
            case ('^'):
                if (code[1]) {
                    code_ch = toupper(*++code) - '@';
                    if (!code_ch)
                        code_ch |= 0200;
                }
                goto default_l;
            case ('\\'):
                code_ch = *(++code);
            default:
            default_l:
                key_code = (unsigned char)code_ch;
                code++;
            }

            // now add the key_code to the tree
            curr = AddToKeyTree(curr, key_code, (*code ? NO_ACTION : ac_table->action),
                                (*code ? NULL : ac_table->arg));
        }
        ac_table++;
    }

    return top;
}

KeyTreeNode *KeyTree = 0;

void FreeKeyTree(KeyTreeNode *kt)
{
    if (!kt)
        return;
    FreeKeyTree(kt->sibling);
    FreeKeyTree(kt->child);
    heap_delete(kt);
}

void RebuildKeyTree()
{
    FreeKeyTree(KeyTree);
    KeyTree = BuildKeyTree(ActionCodeTable);
}

int FindActionCode(const char *ActionName)
{
    int lo = A__FIRST;
    int hi = A__LAST + 1;

    while (lo < hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(ActionName, GetActionName(mid));
        if (cmp == 0)
            return mid;
        if (cmp > 0)
            lo = mid + 1;
        else
            hi = mid;
    }

    // try aliases (there are few, no need for bsearch)
    for (int i = 0; ActionNameAliases[i].alias; i++)
        if (!strcmp(ActionName, ActionNameAliases[i].alias))
            return ActionNameAliases[i].code;

    return -1;
}

int ParseActionNameArg(char *action, const char **arg)
{
    // extract the action parameter
    *arg       = NULL;
    char *end  = action + strlen(action);
    char *par1 = strchr(action, '(');
    if (par1 && end[-1] == ')') {
        *par1   = 0;
        end[-1] = 0;
        *arg    = par1 + 1;
    }
    // convert the action name to code
    return FindActionCode(action);
}

char *ParseActionArgumentAlloc(const char *arg)
{
    if (!arg || !*arg)
        return NULL;
    char *alloc = (char *)malloc(strlen(arg) + 1);
    char *store = alloc;
    while (*arg) {
        switch (*arg) {
        case '_':
            *store++ = ' ';
            arg++;
            break;
        case '\\':
            arg++;
            switch (*arg) {
            case '\0':
                *store++ = '\\';
                break;
            case 'n':
                *store++ = '\n';
                arg++;
                break;
            case 'r':
                *store++ = '\r';
                arg++;
                break;
            case 't':
                *store++ = '\t';
                arg++;
                break;
            default:
                *store++ = *arg++;
            }
            break;
        default:
            *store++ = *arg++;
        }
    }
    *store = 0;
    return alloc;
}

Task<void> ReadActionMap(FILE *f)
{
    FreeActionCodeTable();

    char ActionName[1024];
    const char *ActionArg;
    char ActionCode[256];
    char *store;
    int ch;
    ActionCodeRec *NewTable = NULL;
    int CurrTableSize       = 0;
    int CurrTableCell       = 0;

    for (;;) /* line cycle */
    {
        store = ActionName;
        for (;;) /* action name cycle */
        {
            ch = co_await le_getc(f);
            if (ch == EOF || isspace(ch))
                break;
            if (store - ActionName < (int)sizeof(ActionName) - 1)
                *(store++) = ch;
        }
        *store = 0;

        int action_found = ParseActionNameArg(ActionName, &ActionArg);
        if (action_found == -1) {
            while (ch != '\n' && ch != EOF)
                ch = co_await le_getc(f);
            if (ch == EOF)
                break;
            continue;
        }

        /* skip spaces between action name and action code */
        while (ch != '\n' && ch != EOF && isspace(ch))
            ch = co_await le_getc(f);

        if (ch == EOF || ch == '\n')
            break;

        store = ActionCode;
        for (;;) {
            if (ch == '\\') {
                ch = co_await le_getc(f);
                switch (ch) {
                case ('e'):
                    ch = 27;
                    break;
                case ('n'):
                    ch = 10;
                    break;
                case ('r'):
                    ch = 13;
                    break;
                case ('t'):
                    ch = 9;
                    break;
                case ('b'):
                    ch = 8;
                    break;
                default:
                    if (isdigit(ch)) {
                        le_ungetc(ch, f);
                        Result<u64> o = co_await f->scan_u64(8, 3);
                        if (o.is_ok())
                            ch = (int)o.value();
                    } else {
                        le_ungetc(ch, f);
                        ch = '\\';
                    }
                }
            }
            if (ch == '\000')
                ch = 128;

            if (store - ActionCode < (int)sizeof(ActionCode) - 1)
                *(store++) = ch;

            ch = co_await le_getc(f);
            if (ch == EOF || isspace(ch))
                break;
        }
        *store = 0;

        if (CurrTableSize <= CurrTableCell) {
            if (NewTable == NULL)
                NewTable = (ActionCodeRec *)malloc((CurrTableSize = 16) * sizeof(*NewTable));
            else
                NewTable =
                    (ActionCodeRec *)realloc(NewTable, (CurrTableSize *= 2) * sizeof(*NewTable));
            if (!NewTable) {
                NoMemory();
                co_return;
            }
        }
        NewTable[CurrTableCell].action = action_found;
        NewTable[CurrTableCell].code   = strdup(ActionCode);
        NewTable[CurrTableCell].arg    = ParseActionArgumentAlloc(ActionArg);
        if (NewTable[CurrTableCell].code == NULL) {
            NoMemory();
            co_return;
        }
        CurrTableCell++;
    }

    NewTable = (ActionCodeRec *)realloc(NewTable, (CurrTableCell + 1) * sizeof(*NewTable));
    if (!NewTable) {
        NoMemory();
        co_return;
    }
    NewTable[CurrTableCell].action = -1;
    NewTable[CurrTableCell].code   = NULL;

    ActionCodeTable        = NewTable;
    DynamicActionCodeTable = NewTable;
}

void FreeActionCodeTable()
{
    if (DynamicActionCodeTable) {
        for (int i = 0; DynamicActionCodeTable[i].code; i++)
            free(DynamicActionCodeTable[i].code);
        free(DynamicActionCodeTable);
        DynamicActionCodeTable = 0;
    }
    ActionCodeTable = 0;
}

Task<int> GetNextAction()
{
    unsigned char *store;
    int key;

    store          = StringTyped;
    StringTypedLen = 0;
    *store         = 0;
    ActionArgument = NULL;

    KeyTreeNode *kt = KeyTree;

    /* Upstream had two loops and a delay: a node's children carried the
       shortest gap that could still be part of an escape sequence, and a key
       that did not arrive in time ended the sequence. Nothing here is timed --
       a key is a key -- so what is left is: read one, walk one edge, and stop
       where there is no edge to walk. */
    for (;;) {
        KeyTreeNode *scan;

        if (!kt->child)
            break;

        key = co_await GetKey();

        extern int resize_flag;
        if (resize_flag && kt == KeyTree) {
            resize_flag = 0;
            if (key != ERR)
                ungetch(key);
            CheckWindowResize();
            co_return WINDOW_RESIZE;
        }
        if (key == ERR)
            break;

        if (key <= UCHAR_MAX) {
            *(store++) = key;
            *store     = 0;
            StringTypedLen++;
        }

        for (scan = kt->child; scan; scan = scan->sibling)
            if (scan->keycode == key || (key == 0 && scan->keycode == 128))
                break;
        if (!scan)
            break;
        kt = scan;
    }

    if (kt->action == REFRESH_SCREEN)
        clearok(stdscr, 1); // force repaint for next refresh
    ActionArgument    = kt->arg;
    ActionArgumentLen = xstrlen(ActionArgument);
    co_return (LastActionCode = kt->action);
}

Task<const char *> GetActionArgument(const char *prompt, History *history, const char *help,
                                     const char *title)
{
    static char *buf[A__LAST - A__FIRST + 1];
    static int buf_len[A__LAST - A__FIRST + 1];
    if (ActionArgument)
        co_return ActionArgument;
    char **b         = buf + LastActionCode - A__FIRST;
    int *len         = buf_len + LastActionCode - A__FIRST;
    const int maxlen = 256;
    if (!*b)
        *b = (char *)malloc(maxlen);
    if (!*b) {
        NoMemory();
        co_return NULL;
    }
    int res = co_await getstring(prompt, *b, maxlen - 1, history, len, help, title);
    if (res == -1)
        co_return NULL;
    ActionArgument    = *b;
    ActionArgumentLen = *len;
    co_return *b;
}
