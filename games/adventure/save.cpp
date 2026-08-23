// Re-coding of advent in C: save and restore -- upstream save.c.
#include "serial.h"

// save (III)   J. Gillogly
// save user core image for restarting
//
// Braam has no seek, so the offset upstream used to append this to
// adventure.dat is gone and a save file is only ever a save file.  Upstream
// wrote struct game raw and then the travel lists node by node; here it is one
// field list, visited by Ser to write and De to read.  The travel lists are
// left out: rdata() rebuilds them before restore() runs, and play never
// changes them.
Task<Result<void>> Game::save(Str savfile) // save game state to file
{
    Ser s;

    if (!s.out.append(SAVE_MAGIC))
        co_return Err(Error::NoMemory);
    visit(s);
    if (!s.ok)
        co_return Err(Error::NoMemory);

    Result<i32> fd = Err(Error::NoMemory);
    if (Task<Result<i32>> t = open_at(savfile, SYS_O_WRITE | SYS_O_CREATE | SYS_O_TRUNC))
        fd = co_await t;
    if (fd.is_err()) {
        adv_printf("Cannot write to ");
        adv_puts(savfile);
        adv_putc('\n');
        co_return Err(fd.error());
    }

    Result<void> w = Err(Error::NoMemory);
    if (Task<Result<void>> t = write_all(u32(fd.value()), s.out.str()))
        w = co_await t;
    if (Task<void> c = close_fd(u32(fd.value())))
        co_await c;
    if (w.is_err()) {
        adv_printf("Cannot write to ");
        adv_puts(savfile);
        adv_putc('\n');
        co_return w;
    }

    adv_printf("Saved %u bytes to ", u32(s.out.size()));
    adv_puts(savfile);
    adv_putc('\n');
    co_return {};
}

Task<Result<void>> Game::restore(Str savfile) // restore game from user file
{
    Result<String> r = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_file(savfile))
        r = co_await t;
    if (r.is_err())
        co_return Err(r.error());

    De d{ r.value().str() };
    if (!d.in.starts_with(SAVE_MAGIC))
        co_return Err(Error::Invalid);
    d.pos = SAVE_MAGIC.size();
    visit(d);
    if (!d.ok)
        co_return Err(Error::Invalid);
    co_return {};
}
