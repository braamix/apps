// The one place the editor parks.
//
// Upstream blocked in curses' getch() and read a multibyte character one byte
// at a time. Braam delivers a whole codepoint, so decode() hands back the first
// byte of its UTF-8 and pushes the rest -- which is what keeps insert()'s
// continuation loop, display(), charwidth() and the gap buffer byte-oriented,
// as upstream wrote them.

#include "braam.h"
#include "ehscreen.h"
#include "kernel/key.h"
#include "proc/rt.h"

int eh_resize_flag;

// eh.cpp's, repeated rather than shared: a header holding three defines is
// less than it costs to keep them in one.
#define CTRL_C '\003'
#define CTRL_U '\025'
#define ESC    '\033'

namespace {

// A line, not a key: prompt() primes the field with a whole filename through
// ungetstr(), whose own bound is COLS, and decode() pushes the tail of a UTF-8
// sequence on top of that.
enum { UNGET_MAX = 1024 };

int unget_buf[UNGET_MAX];
int unget_n;

int unget_take()
{
    return unget_n > 0 ? unget_buf[--unget_n] : ERR;
}

// One Braam key as the byte, or the named code, that cmds[] and insert() want.
int decode(Key k)
{
    switch (k.code) {
    // The named keys pass through: they are above the Unicode range, and eh
    // tells them from a byte with its own `255 < ch`.
    case KEY_UP:
    case KEY_DOWN:
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_HOME:
    case KEY_END:
    case KEY_PAGE_UP:
    case KEY_PAGE_DOWN:
    case KEY_DELETE:
    case KEY_BACKSPACE:
        return (int)k.code;

    case KEY_ENTER:
        return '\n';
    case KEY_ESCAPE:
        return ESC;
    // Shift-Tab is a tab. Upstream's `case KEY_BTAB:` sits below default:'s
    // `255 < ch` guard, so a real one would reach mblength() with a value its
    // own assert forbids -- and NDEBUG would let it write four garbage bytes.
    case KEY_TAB:
        return '\t';
    }

    // Ctrl on a printable key is the control character it has always been.
    if (k.mods & MOD_CTRL) {
        int c = k.code;
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 1;
        else if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 1;
        else if (c == '[')
            c = ESC;
        else if (c == '\\')
            c = 28;
        else if (c == ']')
            c = 29;
        else if (c == '^')
            c = 30;
        else if (c == '_' || c == '/')
            c = 31;
        else if (c == '@' || c == ' ')
            c = 0;
        else if (c == '?')
            c = 0177;
        else
            return ERR;
        return c;
    }

    if (k.mods & MOD_ALT)
        return ERR;

    // A printable key, as its UTF-8: hand back the first byte and queue the
    // rest, backwards, since ungetch is a stack.
    {
        char buf[4];
        int n = wctomb(buf, (wchar_t)k.code);

        if (n < 1)
            return ERR;
        for (int i = n - 1; i > 0; i--)
            ungetch((unsigned char)buf[i]);
        return (unsigned char)buf[0];
    }
}

// The geometry rides on every reply, so next_key() may have reshaped the grid
// on either exit.
void note_size()
{
    if ((int)eh_grid().cols != COLS || (int)eh_grid().rows != LINES) {
        eh_resized();
        eh_resize_flag = 1;
    }
}

} // namespace

int ungetch(int key)
{
    if (unget_n >= UNGET_MAX)
        return ERR;
    unget_buf[unget_n++] = key;
    return 0;
}

Task<int> getch()
{
    if (unget_n)
        co_return unget_take();

    for (;;) {
        // Painting sent nothing; this is where the frame goes out, and it must
        // go out before the process parks or the screen would lag a keystroke
        // behind.
        if (Task<Result<void>> t = eh_flush())
            co_await t;

        Result<Key> r = Err(Error::NoMemory);
        if (Task<Result<Key>> t = eh_screen().next_key())
            r = co_await t;

        if (r.is_err()) {
            if (r.error() != Error::Intr)
                co_return ERR;
            // next_key() takes SIG_WINCH itself and reshapes before it reports,
            // so note the geometry before answering whatever is behind this.
            note_size();
            eh_full_blit();
            if (sig_take(SIG_TERM))
                co_return ERR;
            if (sig_take(SIG_INT))
                co_return CTRL_C;
            if (eh_resize_flag)
                co_return ERR;
            continue;
        }

        note_size();
        int key = decode(r.value());
        if (key != ERR)
            co_return key;
        if (unget_n)
            co_return unget_take();
    }
}

// A cooked line, echoed by hand. Upstream used curses' own mvgetnstr() under
// noraw()+echo(); there is no line discipline here, so the erase and kill
// characters are ours -- which is also why upstream's `empty2` test, disabled
// over a tty-vs-pipe backspace difference, is live again here.
//
// `buf` is the gap (prompt(), eh.cpp). Nothing in this loop may move it.
Task<int> mvgetnstr(int y, int x, char *buf, int n, u8 attr)
{
    int len = 0;

    // The field has to be on the screen, which move() used to say.
    if (n < 1 or y < 0 or x < 0 or LINES <= y or COLS <= x)
        co_return ERR;
    for (;;) {
        // The whole field, every turn: it is one row, and repainting picks up a
        // COLS that a resize moved. The caret goes after the text, and the tail
        // is cleared in no attribute -- the underline stops at the field.
        buf[len] = '\0';
        int end  = eh_put(y, x, buf, len, attr);
        eh_fill(y, end, 0);
        eh_cursor(y, end);

        int ch = co_await getch();

        switch (ch) {
        case ERR:
            buf[len] = '\0';
            co_return ERR;
        case '\n':
        case '\r':
        case KEY_ENTER:
            buf[len] = '\0';
            co_return 0;
        case ESC:
        case CTRL_C:
            // An empty field: W and R then beep, and Q reads it as "not y".
            buf[0] = '\0';
            co_return ERR;
        case '\b':
        case 0177:
        case KEY_BACKSPACE:
            while (0 < len and (buf[len - 1] bitand 0300) == 0200) {
                len--;
            }
            if (0 < len) {
                len--;
            }
            break;
        case CTRL_U:
            len = 0;
            break;
        default:
            if (ch < ' ' or 255 < ch) {
                // A named key, or a control this field has no use for.
                break;
            }
            if (n - 1 <= len) {
                beep();
                break;
            }
            buf[len++] = (char)ch;
            break;
        }
    }
}
