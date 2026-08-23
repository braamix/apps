// Re-coding of advent in C: save and restore -- upstream save.c.
#include "hdr.h"

// save (III)   J. Gillogly
// save user core image for restarting
//
// Braam has no seek, so the offset upstream used to append this to
// adventure.dat is gone and a save file is only ever a save file.  The travel
// lists are still written node by node and read back the same way: glorkz is
// parsed before either happens, so the chains already have the right shape and
// the pointers in the file are read only as "there was a node here".
Task<Result<void>> save(Str savfile) // save game state to file
{
    struct travlist *entry;
    String bytes;
    u32 n = 0;

    if (!bytes.append(Str(reinterpret_cast<const char *>(&game), sizeof game)))
        co_return Err(Error::NoMemory);
    n += sizeof game;

    for (int i = 0; i < LOCSIZ; i++) // save travel lists
    {
        entry = game.travel[i];
        while (entry != 0) {
            if (!bytes.append(Str(reinterpret_cast<const char *>(entry), sizeof *entry)))
                co_return Err(Error::NoMemory);
            n += sizeof *entry;
            entry = entry->next;
        }
    }

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
    if (Task<Result<void>> t = write_all(u32(fd.value()), bytes.str()))
        w = co_await t;
    if (Task<void> c = close_fd(u32(fd.value())))
        co_await c;
    if (w.is_err()) {
        adv_printf("Cannot write to ");
        adv_puts(savfile);
        adv_putc('\n');
        co_return w;
    }

    adv_printf("Saved %u bytes to ", n);
    adv_puts(savfile);
    adv_putc('\n');
    co_return {};
}

Task<Result<void>> restore(Str savfile) // restore game from user file
{
    struct travlist **entryp;

    Result<String> r = Err(Error::NoMemory);
    if (Task<Result<String>> t = read_file(savfile))
        r = co_await t;
    if (r.is_err())
        co_return Err(r.error());

    Str s = r.value().str();
    if (s.size() < sizeof game)
        co_return Err(Error::Invalid);
    __builtin_memcpy(&game, s.data(), sizeof game);
    usize at = sizeof game;

    for (int i = 0; i < LOCSIZ; i++) // restore travel lists
    {
        if (!game.travel[i])
            continue;
        entryp = &game.travel[i];
        while (*entryp != 0) {
            if (at + sizeof **entryp > s.size())
                co_return Err(Error::Invalid);
            *entryp = heap_new<struct travlist>();
            if (!*entryp)
                co_return Err(Error::NoMemory);
            __builtin_memcpy(*entryp, s.data() + at, sizeof **entryp);
            at += sizeof **entryp;
            entryp = &(*entryp)->next;
        }
    }
    co_return {};
}
