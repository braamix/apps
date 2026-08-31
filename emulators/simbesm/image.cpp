/*
 * The host build's images, over stdio.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * The Braam build replaces this file with one over descriptors -- seek_fd and
 * read_some -- and nothing above it changes, because a transfer here is already
 * "seek to a word offset, move n words" and is already performed from the
 * driver loop rather than from inside an instruction.
 */
#include "besm6_defs.h"

struct Image {
    FILE *f;
    int err;
};

Image *img_open(const char *path, int create, int must_exist, int roable, int *how, int *why)
{
    Image *m;
    FILE *f;

    *how = create ? IMG_CREATED : IMG_OPENED;
    *why = SCPE_OK;

    if (create) {
        f = fopen(path, "wb+");
        if (!f) {
            *why = SCPE_OPENERR;
            return NULL;
        }
    } else {
        f = fopen(path, "rb+");
        if (!f) {
            if ((errno == EROFS) || (errno == EACCES) || (errno == EPERM)) {
                if (!roable) {
                    *why = SCPE_NORO;
                    return NULL;
                }
                f = fopen(path, "rb");
                if (!f) {
                    *why = SCPE_OPENERR;
                    return NULL;
                }
                *how = IMG_RDONLY;
            } else if (must_exist) {
                *why = SCPE_OPENERR;
                return NULL;
            } else {
                f = fopen(path, "wb+");
                if (!f) {
                    *why = SCPE_OPENERR;
                    return NULL;
                }
                *how = IMG_CREATED;
            }
        }
    }

    m = (Image *)calloc(1, sizeof(*m));
    if (!m) {
        fclose(f);
        *why = SCPE_MEM;
        return NULL;
    }
    m->f = f;
    return m;
}

int img_close(Image *m)
{
    int bad;

    if (!m)
        return 0;
    bad = m->err || (fclose(m->f) == EOF);
    free(m);
    return bad;
}

int img_read(Image *m, uint32 off, t_value *dst, int n)
{
    size_t got;

    if (fseek(m->f, (long)off * 8, SEEK_SET) != 0)
        return 0;
    got = fread(dst, 8, n, m->f);
    if (ferror(m->f))
        m->err = 1;
    return (int)got;
}

int img_write(Image *m, uint32 off, const t_value *src, int n)
{
    size_t put;

    if (fseek(m->f, (long)off * 8, SEEK_SET) != 0)
        return 0;
    put = fwrite(src, 8, n, m->f);
    if (ferror(m->f))
        m->err = 1;
    return (int)put;
}

int img_append(Image *m, const t_value *src, int n)
{
    size_t put = fwrite(src, 8, n, m->f);

    if (ferror(m->f))
        m->err = 1;
    return (int)put;
}

int img_error(Image *m)
{
    return m ? m->err : 0;
}

void img_remove(const char *path)
{
    remove(path);
}
