/*
 * Disk and drum images: a file of 48-bit words, addressed by word.
 *
 * Copyright (c) 2026, Serge Vakulenko
 *
 * Upstream said this with fseek/fread/fwrite on a FILE * in the UNIT; here it
 * is what the machine asks for and the driver performs (machine.h).
 */
#ifndef BESM6_IMAGE_H
#define BESM6_IMAGE_H

#include <stddef.h>

#include "types.h"

typedef struct Image Image;

/* How an image came to be open; the caller reports it. */
enum {
    IMG_OPENED = 0, /* an existing image, read-write */
    IMG_RDONLY,     /* an existing image the store would not give up for writing */
    IMG_CREATED,    /* there was none, and `must_exist' did not insist */
};

/* `create' truncates.  NULL on failure, *why an SCPE_ code. */
Image *img_open(const char *path, int create, int must_exist, int roable, int *how, int *why);

/* Nonzero on a write that could not be completed. */
int img_close(Image *m);

/* Words transferred; short of `n' past the end, which is an unformatted zone
 * and not an error -- every caller reports it to the guest as its own. */
int img_read(Image *m, uint32 off, t_value *dst, int n);
int img_write(Image *m, uint32 off, const t_value *src, int n);

/* Appends: what formatting a fresh image is. */
int img_append(Image *m, const t_value *src, int n);

/* Sticky, as ferror() was. */
int img_error(Image *m);

/* Deletes one rejected after being created. */
void img_remove(const char *path);

/*
 * A whole file in memory, and the sequential reads the a.out loader makes of
 * it.  Those were fgets/getc/fread/rewind over a FILE *; the kernel image is
 * 110 KB and reading it once is what keeps sim_load() a plain function.
 */
typedef struct {
    const unsigned char *base;
    size_t len, pos;
} Blob;

/* Reads `path' whole into a heap block the caller frees.  The platform's. */
int img_slurp(const char *path, Blob *b);
void blob_free(Blob *b);

/* The `ч' line of a .b6 image.  strtod is not in the port kit; math/ftoa.h has
 * it under another name, so the platform answers this. */
double sim_strtod(const char *s, char **end);

int blob_getc(Blob *b); /* -1 at the end */
size_t blob_read(Blob *b, void *p, size_t n);
char *blob_gets(Blob *b, char *s, int n);
void blob_rewind(Blob *b);

#endif /* BESM6_IMAGE_H */
