// LOAD, SAVE, OPEN, CLOSE, and the channel table.
//
// m6502.asm's LOAD AND SAVE SUBROUTINES section plus the EXTIO fragments;
// 05-statements.md §7.4, 11-io-init.md §4, 12-config-matrix.md §§5-6.
//
// These are the two switches this build turns on that the Apple one did not.
// DISKO=1 puts LOAD and SAVE in RESLST and STMDSP, right after WAIT, where the
// source puts them; EXTIO=1 adds INPUT#, PRINT#, CMD, SYS, OPEN and CLOSE and
// the ?FILE DATA error. Both renumber every later token, which is why they
// have to be decided before anything else is written.
//
// Upstream's LOAD and SAVE were the machine's cassette or its KERNAL vectors,
// and the KIM version had to relink the program on load because the line links
// were absolute addresses. A program is LIST-format text here -- the same
// bytes a user would type -- so LOAD re-tokenizes and there is nothing to
// relink.
#include "mbasic.h"

Chan *Interp::chan_find(u8 n)
{
    for (usize i = 0; i < chans.size(); i++)
        if (chans[i].num == n)
            return &chans[i];
    return nullptr;
}

// The file name is a string expression, which is not how any 1978 target
// spelled it -- the KIM took an ID byte and the Commodore a KERNAL argument
// block -- but it is the only spelling that means anything here.
bool Interp::filename(String &name_out)
{
    frmevl();
    CHKV(false);
    chkstr();
    CHKV(false);
    if (fac.s.empty())
        ERRV(false, ERRFD);
    name_out = static_cast<String &&>(fac.s);
    fac      = Val{};
    return true;
}

void Interp::stmt_save()
{
    String name;
    if (!filename(name))
        return;

    // The program as LIST would print it. LIST itself goes to the console and
    // exits to READY, so this walks the same table rather than calling it.
    String text;
    for (usize i = 0; i < prog.size(); i++) {
        const Line &l = prog[i];
        u16 n         = l.num;
        char d[8];
        usize k = 0;
        do {
            d[k++] = char('0' + n % 10);
            n /= 10;
        } while (n);
        while (k)
            if (!reason(text.push(d[--k])))
                return;
        if (!reason(text.push(' ')))
            return;

        for (usize j = 0; j + 1 < l.text.size(); j++) {
            u8 c    = l.text[j];
            bool ok = c < 0x80 ? text.push(char(c))
                               : text.append(usize(c - ENDTK) < RESLST_COUNT ? RESLST[c - ENDTK]
                                                                             : Str("?"));
            if (!reason(ok))
                return;
        }
        if (!reason(text.push('\n')))
            return;
    }

    req.file      = FileReq{};
    req.file.op   = FileOp::Save;
    req.file.name = static_cast<String &&>(name);
    req.file.data = static_cast<String &&>(text);
    SUSPEND(suspend_file(Resume::File));
}

void Interp::stmt_load()
{
    String name;
    if (!filename(name))
        return;
    req.file      = FileReq{};
    req.file.op   = FileOp::Load;
    req.file.name = static_cast<String &&>(name);
    SUSPEND(suspend_file(Resume::File));
}

// OPEN <n>,<name> for input, or OPEN <n>,<name>,"W" for output. Upstream's
// OPEN took the Commodore's file number, device and secondary address; a
// number and a name is what those mean here.
void Interp::stmt_open()
{
    u8 n = getbyt();
    CHK;
    if (n == 0)
        ERR(ERRFC);
    if (chan_find(n))
        ERR(ERRFD);
    if (chrgot() != ',')
        ERR(ERRSN);
    chrget();

    String name;
    if (!filename(name))
        return;

    bool input = true;
    if (chrgot() == ',') {
        chrget();
        frmevl();
        CHK;
        chkstr();
        CHK;
        input = !(fac.s.size() && (fac.s[0] == 'W' || fac.s[0] == 'w'));
        fac   = Val{};
    }

    req.file       = FileReq{};
    req.file.op    = FileOp::Open;
    req.file.num   = n;
    req.file.input = input;
    req.file.name  = static_cast<String &&>(name);
    SUSPEND(suspend_file(Resume::File));
}

void Interp::stmt_close()
{
    u8 n = getbyt();
    CHK;
    Chan *c = chan_find(n);
    if (!c)
        ERR(ERRFD);

    req.file     = FileReq{};
    req.file.op  = FileOp::Close;
    req.file.num = n;
    req.file.fd  = c->fd;
    SUSPEND(suspend_file(Resume::File));
}

// What the driver did, taken up again. Upstream's LOAD finished at FINI so the
// program was relinked; here it is re-tokenized, which is the same thought.
void Interp::file_resume()
{
    FileReq &f = req.file;

    switch (f.op) {
    case FileOp::Load: {
        if (!f.ok)
            ERR(ERRFD);
        scrtch(); // LOAD replaces the program, as NEW does
        Str text = f.data.str();
        usize i  = 0;
        while (i < text.size()) {
            usize e = i;
            while (e < text.size() && text[e] != '\n')
                e++;
            usize n = e - i;
            if (n && text[i + n - 1] == '\r')
                n--;
            if (!load_line(text.substr(i, n)))
                return;
            i = e < text.size() ? e + 1 : e;
        }
        // FINI: RUNC, relink, and back to the prompt. Upstream did not run
        // what it had just loaded, and neither does this.
        runc();
        halt_ = Halt::Ready;
        return;
    }
    case FileOp::Save:
        if (!f.ok)
            ERR(ERRFD);
        return;

    case FileOp::Open: {
        if (!f.ok)
            ERR(ERRFD);
        Chan c;
        c.num   = f.num;
        c.fd    = f.fd;
        c.input = f.input;
        if (f.input) {
            c.ibuf = static_cast<String &&>(f.data);
            c.eof  = true;
        }
        reason(chans.push(static_cast<Chan &&>(c)));
        return;
    }
    case FileOp::Close:
        for (usize i = 0; i < chans.size(); i++)
            if (chans[i].num == f.num) {
                chans.erase(i);
                break;
            }
        return;
    }
}

// One line of a loaded program: crunch it and place it. The editor path in
// crunch.cpp does the same thing, but it also clears the variables and goes
// back to MAIN, which a load in the middle of a file must not.
bool Interp::load_line(Str line)
{
    usize i = 0;
    while (i < line.size() && line[i] == ' ')
        i++;
    if (i == line.size())
        return true;
    if (line[i] < '0' || line[i] > '9')
        ERRV(false, ERRFD);

    u32 n = 0;
    for (; i < line.size(); i++) {
        if (line[i] == ' ')
            continue;
        if (line[i] < '0' || line[i] > '9')
            break;
        n = n * 10 + u32(line[i] - '0');
        if (n > MAXLIN)
            ERRV(false, ERRFD);
    }
    while (i < line.size() && line[i] == ' ')
        i++;

    Vec<u8> text;
    crunch(line.substr(i), text);
    CHKV(false);
    if (text.size() <= 1)
        return true;

    bool found = false;
    usize at   = fndlin(u16(n), found);
    if (found)
        prog.erase(at);

    Line l;
    l.num  = u16(n);
    l.text = static_cast<Vec<u8> &&>(text);
    return reason(prog.insert(at, static_cast<Line &&>(l)));
}
