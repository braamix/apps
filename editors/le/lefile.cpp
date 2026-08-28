// The stdio calls the config parsers make. See lefile.h.

#include "lefile.h"

#include "braam.h"
#include "kernel/alloc.h"
#include "lesys.h"

Task<int> le_getc(FILE *f)
{
    char b;

    if (!f)
        co_return EOF;
    Result<usize> r = co_await f->read({ &b, 1 });

    if (r.is_err() || r.value() == 0)
        co_return EOF;
    co_return (unsigned char)b;
}

void le_ungetc(int c, FILE *f)
{
    if (c != EOF)
        f->unget((char32_t)(unsigned char)c);
}

Task<void> le_putc(int c, FILE *f)
{
    char b = (char)c;

    co_await f->write(Str(&b, 1));
}

Task<void> le_puts(const char *s, FILE *f)
{
    co_await f->write(Str(s, strlen(s)));
}

// A File is move-only and every caller here keeps a pointer, so it lives on
// the heap for as long as the parse does.
Task<FILE *> le_fopen(const char *path, bool write)
{
    Result<File> r =
        co_await File::open(Str(path, strlen(path)), write ? FileMode::Write : FileMode::Read);

    if (r.is_err()) {
        errno = int(r.error());
        co_return nullptr;
    }
    FILE *f = heap_new<File>(move(r.value()));
    if (!f)
        errno = int(Error::NoMemory);
    co_return f;
}

Task<void> le_fclose(FILE *f)
{
    if (!f)
        co_return;
    co_await f->close();
    heap_delete(f);
}
